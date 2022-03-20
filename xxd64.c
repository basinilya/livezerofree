#define _GNU_SOURCE /* for fallocate() */
#include "mylogging.h"

#include <ctype.h>
#include <stdint.h>
#include <stddef.h>
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

static unsigned char printables[256];
static const char hexchars[] = "0123456789abcdef";
static FILE *in;
static FILE *out;
static char sector[512];

#define PART_ADDR "****************: "
#define PART_HEX \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"" \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		"** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** " \
		" "
#define PART_TEXT \
		"________________________________________________________________________________________________________________________________" \
		"________________________________________________________________________________________________________________________________" \
		"________________________________________________________________________________________________________________________________" \
		"________________________________________________________________________________________________________________________________" \
		"\n"

static char line[16+2+(3*512)+1+512+2] = PART_ADDR PART_HEX PART_TEXT;

static size_t my_readfully() {
	size_t nb, total = 0;
	for (;;) {
		nb = fread(&sector, 1, 512 - total, in);
		total += nb;
		if (total == 512 || feof(in) || ferror(in)) {
			break;
		}
	}
	return total;
}

//static void (unsigned char byte
static int my_isprint(int c) {
	int res = isprint(c);
	return res;
}

static void my_write(size_t nb) {
	fwrite(line, 1, nb, out);
}

int main(int argc, char* argv[])
{
	uint64_t pos = 0;
	size_t nb;
	int pos_hex, pos_text;
	int i;
	char hexdigit, letter;
	in = stdin;
	out = stdout;
	if (!freopen(NULL, "rb", in)) {
		pSysError(ERR, "freopen() failed");
		_exit(1);
	}

	for (int c = 0; c < 256; c++) {
		printables[c] = my_isprint(c) ? 1 : 0;
	}

	//pos = 0xAABBCCDDEEFFLL;  // TODO:

	for (;;pos += 512) {
		nb = my_readfully();
		//nb = 500; // TODO:
		if (nb != 0) {
			for (i = 0; i < 16; i++) {
				hexdigit = hexchars[(pos >> (i*4)) & 0xF];
				line[16 - i - 1] = hexdigit;
			}
			pos_hex = sizeof(PART_ADDR) - 1;
			pos_text = sizeof(PART_ADDR) - 1 + sizeof(PART_HEX) - 1;
			for (i = 0; i < nb; i++) {
				unsigned char byte = sector[i];
				hexdigit = hexchars[(byte >> 4) & 0xF];
				line[pos_hex++] = hexdigit;
				hexdigit = hexchars[byte & 0xF];
				line[pos_hex++] = hexdigit;
				pos_hex++;

				letter = printables[byte] ? byte : '.';
				line[pos_text++] = letter;
			}
			for (; i < 512; i++) {
				line[pos_hex++] = ' ';
				line[pos_hex++] = ' ';
				pos_hex++;
				line[pos_text] = '\n';
			}
			my_write(pos_text + 1);
		}
		if (nb != 512) {
			if (ferror(in)) {
				pSysError(ERR, "fread() failed");
				_exit(1);
			}
			break;
		}
	}
	return 0;
}

