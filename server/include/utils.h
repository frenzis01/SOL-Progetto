#ifndef UTILS_H
#define UTILS_H

#include <server.h>

#define ec(s, r)            \
    puts(#s);               \
    if ((s) == (r))         \
    {                       \
        perror(#s);         \
        exit(EXIT_FAILURE); \
    }

#define ec_nz(s,c)            \
    puts(#s);               \
    if (s)                  \
    {                       \
        perror(#s);         \
        c; \
    }

#define re_neg1(s)   \
    puts(#s);        \
    if ((s) == (-1)) \
    {                \
        perror(#s);  \
        return -1;   \
    }

#define ec_neg1(s)          \
    if ((s) == (-1))        \
    {                       \
        perror(#s);         \
        exit(EXIT_FAILURE); \
    }

#define mye_z(s, x)  \
    if (!(s)) \
    {                \
        myerr = x;   \
        perror(#s);  \
    }

#define nz_do(s, x) \
    if ((s))        \
    {               \
        x        \
    }

ssize_t /* Read "n" bytes from a descriptor */
readn(int fd, void *ptr, size_t n);

ssize_t /* Write "n" bytes to a descriptor */
writen(int fd, void *ptr, size_t n);

char *readLineFromFILEn(char *inputfer, unsigned int len, FILE *fp);

char *readLineFromFILE(char *buffer, unsigned int len, FILE *fp);

int myRead(const char *path, char *buf, size_t bufSize);

char *conf_string(FILE *file, char const *desired_name);

int conf_sizet(FILE *file, char const *desired_name, size_t *ret);

sigset_t initSigMask();

int max(int args, ...);

#endif