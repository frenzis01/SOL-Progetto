#ifndef FILESYS_H
#define FILESYS_H

#include <server.h>

// STORAGE HANDLERS

// if (lockF) then lockFile atomica
int openFile(char *path, int createF, int lockF, Client client);

int writeFile(char *path);

int readFile (char *path);

// Se è già O_LOCK, aggiunge client alla coda dei Lockers in attesa
int lockFile(char *path, Client client);

int unlockFile(char *path, Client client);

// A che serve questa append?
int appendTo(char *path, char *msg);

// le operazioni dopo la close falliscono
int closeFile(char *path);

// rimuove path dal server. Fallisce se path !LOCKED o se è LOCKED da un client diverso 
int removeFile(char *path, Client client);


fnode *initFile(char *path);

// Operazioni basilari storage
_Bool storeIsEmpty();

int storeInsert(fnode *fptr);

int storeDelete(fnode *fptr);

fnode* storeSearch(char *path);

int storeDestroy(FileSystem *store);

int freeFile(fnode *fptr);

_Bool storeIsFull();

#endif