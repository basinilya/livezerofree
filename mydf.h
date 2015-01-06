#ifndef _MYPROG_DF_H
#define _MYPROG_DF_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int myprog_df(const char *filename, char *devicename, int devicenamesz);

#ifdef __cplusplus
}
#endif

#endif
