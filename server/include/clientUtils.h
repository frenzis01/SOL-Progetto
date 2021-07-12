#ifndef MY_CLIENTUTILS
#define MY_CLIENTUTILS

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <protocol.h>
#include <utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>

typedef struct
{
    char
        *path,
        *content;
    size_t size;
} evictedFile;

int printEvictedPath(void *arg);

int printEvicted(void *arg);

void freeEvicted(void *arg);

int storeFileInDir(evictedFile *f, const char *dirname);

char *getAbsolutePath(const char *path);

#endif