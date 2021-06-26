#ifndef MY_CONN_H
#define MY_CONN_H

Request *getRequest(int fd,  int *msg);

sigset_t initSigMask();

void freeRequest(void *arg);

Client *addClient(int fd);

int removeClient (int fd);

#endif