#ifndef FILESYS_H
#define FILESYS_H

#include <server.h>
#include <icl_hash.h>
#include <queue.h>

typedef struct {
    pid_t pid;
    int fd;
    // queue *opened;
    // queue *locked; // actually locked or pending ?
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
    char *path;
    char *content;
    size_t size;
} evictedFile;

typedef struct {
    // fnode *head;
    // fnode *tail;
    queue *files;
    icl_hash_t *fdict;
    // NB: fdict cotiene data*, i nodi della queue, non fnode*

    pthread_mutex_t lockStore;

    size_t currNfiles;
    size_t currSize;
    
    size_t maxNfiles;
    size_t maxSize;
} FileSystem;


// STORAGE HANDLERS

int cmpEvicted(void *a, void *b);

int cmpPath(void *a, void *b);

int readFile(char *path, evictedFile **toRet, Client *client, _Bool readN);

int readNfiles(int n, queue **toRet, Client *client);

int cmpPathChar(void *a, void *b);

int appendToFile(char *path, char *content, size_t size, Client *client, queue **evicted, int writeFlag);

int openFile(char *path, int createF, int lockF, Client *client, evictedFile **evicted); /*fnode **toRet,*/

int lockFile(char *path, Client *client);

int unlockFile(char *path, Client *client);



// le operazioni dopo la close falliscono
int closeFile(char *path, Client *client);

// rimuove path dal server. Fallisce se path !LOCKED o se è LOCKED da un client diverso 
int removeFile(char *path, Client *client, evictedFile **evicted);


fnode *initFile(char *path);

int storeInit(size_t maxNfiles, size_t maxSize);

// Operazioni basilari storage
_Bool storeIsEmpty();

int storeInsert(fnode *fptr);

int storeDelete(fnode *fptr, evictedFile **toRet);

fnode* storeSearch(char *path);

evictedFile *storeEviction(_Bool ignoreLocks, char *pathToAvoid);

int storeDestroy();

void printFD(void *arg); // TODO this is debug

void printEvicted(void *arg);

void printFnode(void *arg);

queue *storeCleaner(size_t sizeNeeded, char *pathToAvoid);

evictedFile *copyFnode (fnode *tmp);

void freeFile(void *arg);

void freeEvicted(void *p);

_Bool storeIsFull();



#endif