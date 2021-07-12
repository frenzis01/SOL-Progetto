#ifndef LOGGER_H
#define LOGGER_H

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

// when size is reached we will write on the actual log file
#define MAX_LOG_SIZE 16384

typedef struct {
    char *buf;
    size_t bufLen;
    FILE* file;
} Logger;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
Logger *logger = NULL; // singleton

int LoggerCreate (char *path);
int LoggerDelete (); // Frees memory, but doesn't unlink file
int LoggerLog (char *buf, size_t len); // Sets errno on error
int LoggerFlush ();    // Sets errno on error

#endif