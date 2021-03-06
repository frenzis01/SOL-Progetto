#ifndef UTILS_H
#define UTILS_H

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

// colors
#define REG "\x1b[0m"
#define BBLK "\033[1;30m"
#define BRED "\033[1;31m"
#define BGRN "\033[1;32m"
#define BYEL "\033[1;33m"
#define BBLU "\033[1;34m"
#define BMAG "\033[1;35m"
#define BCYN "\033[1;36m"
#define BWHT "\033[1;37m"


// Logger buf dim
#define LOGBUF_LEN 400
#define ERRNOBUF_LEN 200

#define ec(s, r, c)     \
    do                  \
    {                   \
        if ((s) == (r)) \
        {               \
            perror(#s); \
            c;          \
        }               \
    } while (0);

#define ec_n(s, r, c)   \
    do                  \
    {                   \
        if ((s) != (r)) \
        {               \
            perror(#s); \
            c;          \
        }               \
    } while (0);

#define ec_nz(s, c)     \
    do                  \
    {                   \
        if ((s))        \
        {               \
            perror(#s); \
            c;          \
        }               \
    } while (0);

#define ec_neg1(s, c)    \
    do                   \
    {                    \
        if ((s) == (-1)) \
        {                \
            perror(#s);  \
            c;           \
        }                \
    } while (0);

#define ec_neg(s, c)    \
    do                  \
    {                   \
        if ((s) < 0)    \
        {               \
            perror(#s); \
            c;          \
        }               \
    } while (0);

#define ec_z(s, c)      \
    do                  \
    {                   \
        if (!(s))       \
        {               \
            perror(#s); \
            c;          \
        }               \
    } while (0);

#define eq_z(s, e, c)                                   \
    do                                                  \
    {                                                   \
        if (!errno && !(s))                             \
        {                                               \
            errno = e;                                  \
            perror(BRED #s REG); \
            c;                                          \
        }                                               \
    } while (0);

#define eok(c)      \
    do              \
    {               \
        if (!errno) \
        {           \
            c;      \
        }           \
    } while (0);

#define ec_z_f(s, c)                                    \
    do                                                  \
    {                                                   \
        if (!errno && !(s))                             \
        {                                               \
            perror(BRED #s REG); \
            c;                                          \
        }                                               \
    } while (0);

ssize_t /* Read "n" bytes from a descriptor */
readn(int fd, void *ptr, size_t n);

ssize_t /* Write "n" bytes to a descriptor */
writen(int fd, void *ptr, size_t n);

void freeNothing(void *arg) { return; }

char *conf_string(FILE *file, char const *desired_name);

int conf_sizet(FILE *file, char const *desired_name, size_t *ret);

int max(int args, ...);

int isInteger(const char *s, int *n);

int isLong(const char* s, long* n);

int readFromDisk(const char *path, void **toRet, size_t *size);

#endif