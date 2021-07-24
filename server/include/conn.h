#ifndef MY_CONN_H
#define MY_CONN_H


#define _POSIX_C_SOURCE 200809L

#include <icl_hash.h>
#include <utils.h>
#include <protocol.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

typedef struct {
    int fd;
} Client;

typedef struct {
    char op;         // mandatory
    char *path;     // mandatory
    char *append;   // buf to be appended
    char *dirname;  // dir for evicted file
    _Bool o_creat, o_lock;  // O_CREAT and O_LOCK
    unsigned int nfiles; // per readNfiles

    unsigned short pathLen; 
    unsigned short dirnameLen;
    size_t appendLen;
    
    Client *client;
} Request;

#define UNLOCKCLIENTS pthread_mutex_unlock(&lockClients)
#define LOCKCLIENTS pthread_mutex_lock(&lockClients)
#define REQstr_LEN PATH_MAX + PATH_MAX + 100

icl_hash_t *clients;
pthread_mutex_t lockClients;


int cmpClient(void *a, void *b);

Request *getRequest(int fd);

sigset_t initSigMask();

void freeRequest(void *arg);

Client *addClient(int fd);

Client *getClient(int fd);

Client *wrapClient(int fd);

_Bool NoMoreClients();

char *reqToString(Request *req, int fd);


#endif