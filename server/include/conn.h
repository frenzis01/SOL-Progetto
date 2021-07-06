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
    // queue *opened;
    // queue *locked; 
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

icl_hash_t *clients;
pthread_mutex_t lockClients;


Request *getRequest(int fd);

sigset_t initSigMask();

void freeRequest(void *arg);

Client *addClient(int fd);

// int removeClient(int fd, queue **notifyLockers);

_Bool NoMoreClients();

void printRequest (Request *req);


#endif