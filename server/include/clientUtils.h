#ifndef MY_CLIENTUTILS
#define MY_CLIENTUTILS

#define _POSIX_C_SOURCE 200809L

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

int printEvicted(void *arg);

void freeEvicted(void *arg);

int storeFileInDir(evictedFile *f, const char *dirname);

#endif