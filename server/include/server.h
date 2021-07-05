#ifndef SERVER_H
#define SERVER_H

// #define _GNU_SOURCE
// #define _POSIX_C_SOURCE 200112L
#define _POSIX_C_SOURCE 200809L
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
// #include <ctype.h>
// #include <stddef.h>
// #include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
// #include <time.h>
#include <unistd.h>
// #include <sys/wait.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <poll.h>
#include <sys/select.h>
#include <stdarg.h>
// #include <sys/mman.h>


#include <filesys.h>
#include <logger.h>
#include <utils.h>
#include <icl_hash.h>
#include <queue.h>
#include <conn.h>
#include <protocol.h>


typedef struct
{
    // config
    size_t workers;
    size_t capacity;
    size_t nfiles;
    size_t evictPolicy;
    char *sockname;

} ServerData;


void *worker(void *arg);

int readConfig(char *configPath, ServerData *new);

int parseRequest (char *str, Request *req);

void *dispatcher(void *arg);



#endif