#ifndef UTILS_H
#define UTILS_H

#include <server.h>
#include <signal.h>


#define ec(s, r, c) \
    /*puts(#s);*/   \
    if ((s) == (r)) \
    {               \
        perror(#s); \
        c;          \
    }

#define ec_n(s, r, c) \
    /*puts(#s);*/     \
    if ((s) != (r))   \
    {                 \
        perror(#s);   \
        c;            \
    }

#define ec_nz(s, c) \
    /*puts(#s);*/   \
    puts(#s);       \
    if (s)          \
    {               \
        perror(#s); \
        c;          \
    }

#define ec_neg1(s, c) \
    /*puts(#s);*/     \
    if ((s) == (-1))  \
    {                 \
        perror(#s);   \
        c;            \
    }

#define ec_z(s, c)  \
    /*puts(#s);*/   \
    if (!(s))       \
    {               \
        perror(#s); \
        c;          \
    }

#define mye_z(s, x) \
    if (!(s))       \
    {               \
        myerr = x;  \
        perror(#s); \
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

// sigset_t initSigMask();

int max(int args, ...);

#endif