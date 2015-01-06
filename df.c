#include "mylogging.h"
#include "mydf.h"

#include <stdio.h>
#include <sys/wait.h> /* for wait() */
#include <stdlib.h> /* for exit() */
#include <unistd.h> /* for execlp() */

#include "mylastheader.h"

int myprog_df(const char *filename, char *devicename, int devicenamesz)
{
	int rc;
	pid_t pid;
	int fds[2];

	if (-1 == pipe(fds)) {
		pSysError(ERR, "pipe() failed");
		return -1;
	}

	rc = -1;
	pid = fork();
	if (pid == -1) {
		pSysError(ERR, "fork() failed");
		goto ennd1;
	}
	if (pid == 0) {
		// child
		close(fds[0]);
		if (-1 == dup2(fds[1], 1)) {
			perror("dup2() failed");
		} else {
			close(fds[1]);
			if (-1 == execlp("df", "df", "--", filename, NULL)) {
				perror("execlp() failed");
			}
		}
		exit(127);
	} else {
		// parent
		int status;
		FILE *in;
		char format[20];
		int n, n2;
		int nitems;

		close(fds[1]);

		in = fdopen(fds[0], "r");
		if (!in) {
			pSysError(ERR, "fdopen() failed");
			goto ennd2;
		}

		// read first word in last line
		devicenamesz--;
		sprintf(format, "%%%d[^ \n]%%n%%*[^\n]\n", devicenamesz);

		n = devicenamesz;
		for(;;) {
			n2 = n;
			n = devicenamesz;
			nitems = fscanf(in, format, devicename, &n);
			if (nitems == EOF) break;
		}

		if (ferror(in)) {
			pSysError(ERR, "reading df output failed");
			goto ennd3;
		}
		if (n2 == devicenamesz || devicename[0] != '/') {
			log(ERR, "parse error");
			goto ennd3;
		}

		do {
			if (-1 == waitpid(pid, &status, 0)) {
				pSysError(ERR, "waitpid() failed");
				goto ennd3;
			}
		} while(!WIFEXITED(status));
		rc = WEXITSTATUS(status) == 0 ? 0 : -1;

		ennd3:
		fclose(in);

		return rc;
	}

	ennd1:
	close(fds[1]);

	ennd2:
	close(fds[0]);

	return rc;
}
