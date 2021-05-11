#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>
#include <stdio.h>
#include <pthread.h>
#include <utils.h>
#include <time.h>
#include <limits.h>

#define E_LOG_MALLOC 1
#define E_LOG_FILE 2
#define E_LOG_MUTEX 3
#define E_LOG_ERROR 4 //GENERAL

// when size is reached we will write on the actual log file
#define MAX_LOG_SIZE 16384

typedef struct {
    char *buf;
    size_t bufLen;
    FILE* file;
} Logger;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
Logger *logger = NULL; // singleton

void LoggerCreate (int *error, char *path);
void LoggerDelete (int *error); // Libera memoria, ma non cancella il file!
void LoggerLog (char *buf, size_t len, int *err); // returns error
void LoggerFlush (int *err); // returns error

#endif