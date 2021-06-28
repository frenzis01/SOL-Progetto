#ifndef MY_CONN_H
#define MY_CONN_H


#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <icl_hash.h>
#include <utils.h>
#include <protocol.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int fd;
    // queue *opened;
    // queue *locked; 
} Client;
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

icl_hash_t *clients;


Request *getRequest(int fd,  int *msg);

sigset_t initSigMask();

void freeRequest(void *arg);

Client *addClient(int fd);

int removeClient (int fd);

_Bool NoMoreClients();


#endif