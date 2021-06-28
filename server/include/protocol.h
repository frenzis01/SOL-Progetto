#ifndef MY_PROTOCOL_H
#define MY_PROTOCOL_H

#include <stdlib.h>

/**
 * Request structure: (without ';')
 *  {1B_op;1B_oflag;1B_nfiles;1B_pathLen;1B_appendLen;1B_dirnameLen;\
 *   pathLen_path;appendLen_append;dirnameLen_dirname}
 * 
 * Response structure:
 *  {RESPONSE CODE | 10B_Nevicted;...evictedFiles...}
 * 
 * Evicted files structure:
 *  {10B_pathLen,path,10B_size,content}
 *  
 */

// #define OPENCONN 1
// #define CLOSECONN 2
#pragma region
#define OPEN_FILE 1
#define READ_FILE 2
#define READN_FILES 3
#define APPEND 4
#define WRITE_FILE 5
#define LOCK_FILE 6
#define UNLOCK_FILE 7
#define CLOSE_FILE 8
#define REMOVE_FILE 9
#pragma endregion

// Operations response codes
// #define CODE_LEN 1
#define SUCCESS 0
#define FILE_NOT_FOUND 1
#define NO_ACCESS 2
#define TOO_BIG 3
#define ALREADY_EXISTS 4

#define MSGLEN_DIM 10

// #define NO_FLAGS 0
#define ONLY_CREAT 1
#define ONLY_LOCK 2
#define BOTH_FLAGS 3

#define NUM_LEN 10

#define INT_LEN 7


char *sockName = "";

#define SZCHAR sizeof(char)

#endif