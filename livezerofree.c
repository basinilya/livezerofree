#define _GNU_SOURCE /* for fallocate() */
#include "mylogging.h"
#include "mydf.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h> /* for asctime() */
#include <fcntl.h> /* for fallocate() */
#include <linux/fs.h> /* for FIBMAP */
#include <sys/ioctl.h> /* for ioctl() */
#include <unistd.h> /* for write(), sync() */
#include <sys/statfs.h> /* for statfs() */

#include "mylastheader.h"

enum { _off_t_is_64 = 1/(sizeof(off_t)-4) };

static char human_size1(double *pSize) {
	static const char units[] = "BKMGT";
	const char *unit = units;

	while (*pSize > 1800 && *(unit + 1) != '\0') {
		*pSize /= 1024;
		unit++;
	}
	return *unit;
}

#define FMT_JUST_BYTES "%lld bytes"

static char *human_size2(char buf[100], long long llsize) {
	double size = llsize;
	char c = human_size1(&size);
	if (c == 'B') {
		sprintf(buf, FMT_JUST_BYTES, llsize);
	}
	else {
		sprintf(buf, FMT_JUST_BYTES "(%.1f%ciB)", llsize, size, c);
	}
	return buf;
}

static long long cwd_free_space(int *block_size) {
	struct statfs st;
	if (0 != statfs(".", &st)) {
		pSysError(ERR, "statfs() failed");
		return 0;
	}
	*block_size = st.f_bsize;
	return st.f_bavail;
}

static size_t dirname_len(char *path)
{
	char *p = path;
	size_t len = 0, sublen = 0;

	for (;; p++) {
		if (*p == 0) return len;
		sublen++;
		if (*p == '/' || *p == '\\') {
			len += sublen;
			sublen = 0;
		}
	}
}

enum { CONWIDTH = 80 };

typedef struct zerofree_data_t {
	long long lastticks;
	double progressdivizor;
	long long curblock;
	long long nfileblocks;
} zerofree_data_t;

static void print_progress(zerofree_data_t *progress, int force) {
	long long ticks;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ticks = (long long)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);

	if (force || ticks - progress->lastticks > 1000) {
		FILE *out = stdout;
		int k;
		int progr;

		progress->lastticks = ticks;

		putc('\r', out);
		putc('|', out);
		if (progress->curblock - progress->nfileblocks < 0) {
			progr = (int)(progress->curblock / progress->progressdivizor);
		} else {
			progr = CONWIDTH - 3;
		}
		for (k = 0; k < progr; k++) {
			putc('=', out);
		}
		for (; k < (CONWIDTH - 3); k++) {
			putc(' ', out);
		}
		putc('|', out);
		fflush(out);
	}
}

#define MAX_PATH 1024

static void time_t_asctime(char buf1[100], time_t timestamp)
{
	struct tm tm;
	localtime_r(&timestamp, &tm);

	asctime_r(&tm, buf1);
}

#define HUMAN_SIZE(llsize) human_size2(smallbuf, llsize)

int main(int argc, char* argv[])
{
	typedef char bigblock_t[16384];
	static const bigblock_t zeroblock = { 0 };

	bigblock_t buf;
	int rc;
	size_t sz;
	char *filename, *origfilename;
	char extrafile[MAX_PATH];
	int n_extrafile = 2, needmorefiles;

	int fdFile;
	FILE *fdev;
	int		block_size;
	char timestring[100], smallbuf[256];
	size_t timestringlen;

	zerofree_data_t data;

	FILE *out = stdout;

	setvbuf(out, NULL, _IOFBF, BUFSIZ); // no autoflush stdout

	if (argc != 2) {
		log(ERR, "bad args. usage: progname FILE");
		return 1;
	}

	filename = argv[1];

	// chdir to file's directory
	sz = dirname_len(filename);
	if (sz != 0) {
		char save = filename[sz];
		filename[sz] = '\0'; /* keep last slash (for "/") */
		if (0 != chdir(filename)) {
			pSysError(ERR, "chdir('" FMT_S "') failed: ", filename);
			return 1;
		}
		filename[sz] = save;
		filename += sz;
	}

	if (0 != myprog_df(".", smallbuf, sizeof(smallbuf))) {
		return 1;
	}
	log(INFO, "device: %s", smallbuf);

	fdev = fopen(smallbuf, "rb");
	if (!fdev) {
		pSysError(ERR, "cannot open device");
		return 1;
	}
	setvbuf(fdev, NULL, _IOFBF, 10*1024*1024);

	rc = 1;

	// we create multiple files due to FS restictions
	origfilename = filename;
	for (;;) {
		long long nfreeblocks, nminfreeblocks, nfilebytes, byteswrote = 0, fdpos = 0, newpos;
		if (0 != unlink(filename)) {
			if (errno != ENOENT) {
				pSysError(ERR, "unlink() failed");
				goto ennd;
			}
		}

		nfreeblocks = cwd_free_space(&block_size);

		log(INFO, "free space: %s, block size: %d", HUMAN_SIZE(nfreeblocks * block_size), block_size);

		if (block_size > sizeof(zeroblock)) {
			log(ERR, "block size too big: %d", block_size);
			return 1;
		}

		// leave untouched at most 10MiB, at least 10 percent
		data.nfileblocks = nfreeblocks;
		nminfreeblocks = 10 * 1024 * 1024 / block_size;
		nfreeblocks = nfreeblocks / 10;
		if (nfreeblocks > nminfreeblocks)
			nfreeblocks = nminfreeblocks;

		data.nfileblocks = data.nfileblocks - nfreeblocks;

		if (data.nfileblocks == 0) {
			log(ERR, "not enough free space");
			return 1;
		}

		fdFile = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
		if (-1 == fdFile) {
			pSysError(ERR, "open('" FMT_S "') failed", filename);
			return 1;
		}

		if (0 != unlink(filename)) {
			if (errno != ENOENT) {
				pSysError(ERR, "unlink() failed");
				return 1;
			}
		}

		// Max file size might be less than free space
		needmorefiles = 0;

		for (;;) {
			nfilebytes = data.nfileblocks * block_size;
			if (0 == fallocate(fdFile, 0, 0, nfilebytes))
				break;

			needmorefiles = 1;
			data.nfileblocks = (data.nfileblocks / 2);

			if (data.nfileblocks == 0) {
				pSysError(ERR, "fallocate() failed");
				goto ennd;
			}
		}

/*
		if (fstat(fd, &statinfo) < 0) {
			pSysError(ERR, "fstat failed");
			return 1;
		}
		num_blocks = (statinfo.st_size + block_size - 1) / block_size;
*/
		lseek(fdFile, 0, SEEK_SET);
		time_t_asctime(smallbuf, time(NULL));
		timestringlen = sprintf(timestring, "zerofree first block, %s", smallbuf);
		rc = write(fdFile, timestring, timestringlen);
		sync();

		log(INFO, "file created: %s: " FMT_S, HUMAN_SIZE(nfilebytes), filename);

		log(INFO, "start rewriting non-zero blocks");
//printf("aaaa\n");
//return 9;
		data.curblock = 0;
		data.progressdivizor = (double)data.nfileblocks / (CONWIDTH - 3);

		print_progress(&data, 1);

		for (;;data.curblock++) {
			int done;

			long long blknum64;
			unsigned int blknum;

			done = data.curblock == data.nfileblocks;
			print_progress(&data, done);
			if (done) break;

			blknum = data.curblock;

			if (ioctl(fdFile, FIBMAP, &blknum)) {
				putc('\n', out);
				pSysError(ERR, "ioctl FIBMAP failed");
				return 1;
			}
			blknum64 = blknum;

			if (-1 == fseeko(fdev, blknum64 * block_size, SEEK_SET)) {
				putc('\n', out);
				pSysError(ERR, "fseeko() failed");
				return 1;
			}
			if (1 != fread(buf, block_size, 1, fdev)) {
				putc('\n', out);
				pSysError(ERR, "fread() failed");
				return 1;
			}
			if (data.curblock == 0) {
				if (0 != memcmp(buf, timestring, timestringlen)) {
					putc('\n', out);
					log(WARN, "first file block has unexpected content");
					log(WARN, "expected: %.*s", timestringlen, timestring);
					log(WARN, "actual  : %.*s", timestringlen, (const char*)buf);
					//log(WARN, "to see actual: dd if=/dev/loop0 bs=%d count=1 skip=%lld 2>/dev/null | less", block_size, blknum64);
				}
			}
			if (0 != memcmp(buf, zeroblock, block_size)) {
				newpos = data.curblock * block_size;
				if (fdpos != newpos) {
					lseek(fdFile, newpos, SEEK_SET);
				}
				write(fdFile, zeroblock, block_size);
				fdpos = newpos + block_size;
				byteswrote += block_size;
			}
		}

		// newline after progress bar
		putc('\n', out);
		fflush(out);

		log(INFO, "wrote: %s", HUMAN_SIZE(byteswrote));

		if (!needmorefiles)
			break;
		snprintf(extrafile, MAX_PATH
			, "%.*s.%d"
			, MAX_PATH - 10
			, origfilename
			, n_extrafile);

		filename = extrafile;
		n_extrafile++;
		// Intentionally not closing fdFile
	}
	rc = 0;
ennd:
	return rc;
}
