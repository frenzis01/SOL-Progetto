#ifndef SERVER_H
#define SERVER_H

#include <queue.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>

#define _GNU_SOURCE
// #define _POSIX_C_SOURCE 200112L
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
// #include <sys/mman.h>


#include <filesys.h>
#include <logger.h>
#include <utils.h>
#include <icl_hash.h>

#define PATH_LENGTH 2048

typedef struct
{
    // config
    size_t workers;
    size_t capacity;
    size_t nfiles;
    size_t evictPolicy;
    char *sockname;

    // stats
    // size_t maxSimultaneousConnections;
    // size_t maxFilesInStorage;
    // size_t maxSizeReached;
} ServerData;



typedef struct {
    int op;         // mandatory
    char *path;     // mandatory
    char *append;   // buf to be appended
    char *dirname;  // dir for evicted file
    int o_creat, o_lock;  // O_CREAT and O_LOCK
    int nfiles; // per readNfiles

    unsigned short pathLen; 
    unsigned short appendLen;
    unsigned short dirnameLen;
    
    Client *client;
} Request;

// Server Data
icl_hash_t *clients; // ?


void *worker(void *arg);

int readConfig(char *configPath, ServerData *new);

int parseRequest (char *str, Request *req);

void *dispatcher(void *arg);

// Elimina file dal server eseguendo LFU finché non c'è 'size' spazio libero
// Predilige file che non sono stati lockati o sceglie proprio quelli?
// Cleanup LFU
int lfuTrasher(int size, int fd);


#endif