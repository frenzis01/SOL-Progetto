#ifndef FILESYS_H
#define FILESYS_H

#include <icl_hash.h>
#include <queue.h>
#include <logger.h>

#include <pthread.h>
#include <utils.h>
#include <conn.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>


typedef struct nodo {
    char *path; // file metadata
    char *content;
    size_t size;

    int fdCanWrite; // fd of the client who requested openFile(O_CREAT,O_LOCK)

    int lockedBy;   // fd of the (client) O_LOCK owner 
    queue *lockersPending; // clients who are waiting to acquire O_LOCK (FIFO)
    queue *openBy; // clients who successfully requested an openFile(...)

    int readers; // Readers/Writers on each file
    int writers; 
    pthread_cond_t go;
    pthread_mutex_t mutex;
    pthread_mutex_t ordering;
} fnode;

typedef struct {
    char *path;
    char *content;
    queue *notifyLockers; // clients who were attempting to acquire the lock
    size_t size;
} evictedFile;

typedef struct {
    queue *files;
    icl_hash_t *fdict;
    // NB: fdict cotiene data*, i nodi della queue, non fnode*

    pthread_mutex_t lockStore;

    size_t currNfiles;
    size_t currSize;
    
    size_t maxNfiles;
    size_t maxSize;

    size_t evictPolicy; // 0 FIFO | 1 LRU

    //stats
    size_t nEviction;
    size_t maxSizeReached;
    size_t maxNfilesReached;
} FileSystem;


FileSystem store;

#define PATH_LENGTH 2048

// filesys
int openFile(char *path, int createF, int lockF, Client *client, evictedFile **evicted); /*fnode **toRet,*/

int readFile(char *path, evictedFile **toRet, Client *client, _Bool readN);

int readNfiles(int n, queue **toRet, Client *client);

int appendToFile(char *path, char *content, size_t size, Client *client, queue **evicted, int writeFlag);

int lockFile(char *path, Client *client);

int unlockFile(char *path, Client *client);

int closeFile(char *path, Client *client);

int removeFile(char *path, Client *client, evictedFile **evicted);

// store
int storeInit(size_t maxNfiles, size_t maxSize, size_t evictPolicy);

int storeDestroy();

// filesys struct - prints and frees 

void printEvicted(void *arg);

void printFnode(void *arg);

void printPath (void *arg);

int storeStats();

void freeFile(void *arg);

void freeEvicted(void *p);



#endif