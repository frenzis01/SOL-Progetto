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
// #include <pthread.h>
// #include <sys/socket.h>
// #include <sys/un.h>
// #include <signal.h>
// #include <poll.h>
// #include <sys/mman.h>


#include<logger.h>
#include <utils.h>

#define PATH_LENGTH 2048

typedef struct
{
    // config
    size_t workers;
    size_t capacity;
    size_t nfiles;
    char *sockname;

    // stats
    // size_t maxSimultaneousConnections;
    // size_t maxFilesInStorage;
    // size_t maxSizeReached;
} ServerData;

typedef struct {
    pid_t pid;
    int fd;
    queue *opened;
    queue *locked; // actually locked or pending ?
} Client;

typedef struct nodo {
    char *path;
    char *content;
    size_t size;

    int fdCanWrite; // 0 : no one
                    // !0 : fd di client che ha fatto openFile(O_CREAT,O_LOCK)

    // Client lockedBy; // Client che ha lockato il file
    int lockedBy;   // 
    queue *lockersPending; // FIFO di Client che attendono la lock
    queue *openBy;

    int readers; // Per condition variable
    int writers; // se c'è un writer attivo

    pthread_cond_t go;
    // mutua esclusione sul singolo file
    pthread_mutex_t mutex;
    pthread_mutex_t ordering;

    // struct nodo *prev;
    // struct nodo *next;
} fnode;

typedef struct {
    // fnode *head;
    // fnode *tail;
    queue *files;

    pthread_mutex_t lockStore;

    size_t currNfiles;
    size_t currSize;
    
    size_t maxNfiles;
    size_t maxSize;
} FileSystem;

// Serve davvero ? O facciamo leggere tutto al worker?
typedef struct {
    char *query;
    Client client;
} Request;

// Server Data
ServerData serverData;

// Storage
FileSystem store;

void *worker(void *arg);

int parseRequest (char *str, Request *req);

void *dispatcher(void *arg);

// Elimina file dal server eseguendo LFU finché non c'è 'size' spazio libero
// Predilige file che non sono stati lockati o sceglie proprio quelli?
// Cleanup LFU
int lfuTrasher(int size, int fd);

ServerData readConfig(char *configPath);

#endif