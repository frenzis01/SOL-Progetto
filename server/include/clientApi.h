#ifndef MY_CLIENTAPI
#define MY_CLIENTAPI

#define _POSIX_C_SOURCE 200809L

#include <protocol.h>
#include <utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <limits.h>
#include <stdarg.h>


int openConnection(const char* sockname, int msec,const struct timespec abstime);
int closeConnection(const char* sockname);
int openFile(const char* pathname, int flags);
int readFile(const char* pathname, void** buf, size_t*size);
int readNFiles(int N, const char* dirname);
int writeFile(const char* pathname, const char*dirname);
int appendToFile(const char* pathname, void* buf,size_t size, const char* dirname);
int lockFile(const char* pathname);
int unlockFile(const char* pathname);
int closeFile(const char* pathname);
int removeFile(const char* pathname);

#endif