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

// output colors
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

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
            perror(ANSI_COLOR_RED #s ANSI_COLOR_RESET); \
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
            perror(ANSI_COLOR_RED #s ANSI_COLOR_RESET); \
            c;                                          \
        }                                               \
    } while (0);

ssize_t /* Read "n" bytes from a descriptor */
readn(int fd, void *ptr, size_t n);

ssize_t /* Write "n" bytes to a descriptor */
writen(int fd, void *ptr, size_t n);

void freeNothing(void *arg) { return; }

char *readLineFromFILEn(char *inputfer, unsigned int len, FILE *fp);

char *readLineFromFILE(char *buffer, unsigned int len, FILE *fp);

int myRead(const char *path, char *buf, size_t bufSize);

char *conf_string(FILE *file, char const *desired_name);

int conf_sizet(FILE *file, char const *desired_name, size_t *ret);

// sigset_t initSigMask();

int max(int args, ...);

int isInteger(const char *s, int *n);

#endif