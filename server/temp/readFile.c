/* 

*/

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <sys/syscall.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/mman.h>

#define SOCKNAME "./mysock"
#define UNIX_PATH_MAX 108

#define BUF 2048

// Macro controllo errori
#define ec(s, r, m)         \
    if ((s) == (r))         \
    {                       \
        perror(m);          \
        exit(EXIT_FAILURE); \
    }

#define ec_n(s, r, m)       \
    if ((s) != (r))         \
    {                       \
        perror(m);          \
        exit(EXIT_FAILURE); \
    }

#define ec_neg1(s)          \
    if ((s) == -1)          \
    {                       \
        perror("myMacro");  \
        exit(EXIT_FAILURE); \
    }

#define re_neg1(s)  \
    if ((s) == -1)  \
    {               \
        perror(#s); \
        return -1;  \
    }

ssize_t /* Read "n" bytes from a descriptor */
readn(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nread;

    nleft = n;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; /* error, return -1 */
            else
                break; /* error, return amount read so far */
        }
        else if (nread == 0)
        {
            puts("eof");
            break; /* EOF */
        }
        nleft -= nread;
        ptr += nread;
    }
    puts("fineciclo");
    // puts("ritorna readn");
    return (n - nleft); /* return >= 0 */
}

ssize_t /* Write "n" bytes to a descriptor */
writen(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;

    nleft = n;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; /* error, return -1 */
            else
                break; /* error, return amount written so far */
        }
        else if (nwritten == 0)
            break;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n - nleft); /* return >= 0 */
}

// Utilities
char *readLineFromFILE(char *inputfer, unsigned int len, FILE *fp)
{
    char *res = fgets(inputfer, len, fp);

    if (res == NULL)
        return res;

    return inputfer;
}

// puts file content in buf
// !!! if file is bigger than bufSize, it does nothing
int myRead(const char *path, char *buf, size_t bufSize)
{
    // path assoluto richiesto
    int fd, bytesRead;
    re_neg1(fd = open(path, O_RDONLY)); // e se venisse interrotta da un segnale?
    // if (fd = open(path,O_RDONLY)) return 0;
    re_neg1(bytesRead = readn(fd, buf, bufSize));
    re_neg1(close(fd)); // e se venisse interrotta da un segnale?
    return bytesRead;
}

// let's try mmap
int myMmapR(const char *path, void **buf, size_t bufSize)
{
    // path assoluto richiesto
    int fd;
    re_neg1(fd = open(path, O_RDWR)); // e se venisse interrotta da un segnale?
    int *ptr = mmap(NULL, bufSize * sizeof(char), PROT_READ | PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        perror("myMmapR");
        return -1;
    }
    close(fd);
    // possiamo chiudere safely il file

    *buf = ptr; // assign ptr to buf

    return 1;
}

int myMmapW(const char *path, void *buf, size_t bufSize, char *msg)
{
    // path assoluto richiesto
    int fd;
    off_t pos = 0;
    re_neg1(fd = open(path, O_RDWR)); // e se venisse interrotta da un segnale?
    re_neg1(pos = lseek(fd, 0, SEEK_END));
    // re_neg1(lseek(fd, strlen(msg), SEEK_SET));
    // re_neg1(write(fd, "", 1));
    // re_neg1(lseek(fd, pos, SEEK_SET));
    printf("--%ld--\n", pos);

    // bisognerebbe controllare le size
    char *ptr = mmap(NULL, (strlen(msg) + 1) * sizeof(char), PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
    {
        close(fd);
        perror("myMmapW_");
        return -1;
    }
    close(fd);

    // printf("OLDMAP : %s", (char*)buf);

    strncpy(ptr+pos, msg, strlen(msg));

    munmap(ptr, strlen(msg) + 1);

    // MMAP GIÀ FATTA NELLA READ
    // buf punta alla regione di memoria su cui è stata chiamata la mmap
    // char *c = (char*)buf;
    // while (*c){
    //      printf("%c",*c);
    //     c++;
    // }
    // // printf("c : %s -",c);
    // // c = calloc(strlen(msg)+1,sizeof(char));
    // memcpy(c,msg,strlen(msg));
    // printf("c : %s -",c);

    return 1;
}

// test rapido
int main(void)
{
    char *buf = calloc(1000, sizeof(char));
    int bytesRead = myRead("test", (char *)buf, 1000);
    printf("%ld : %d : %s", sizeof(buf), bytesRead, buf);
    free(buf);

    // char *buf;
    //buf = calloc (1000,sizeof(char));
    re_neg1(myMmapR("test", (void **)(&buf), 1000));
    printf("MMap : %s", buf);
    re_neg1(myMmapW("test", buf, 1000, "OH PAOLO"));
    printf("Dopo la write : %s", buf);
    munmap(buf, 1000);
    // free(buf);
    return 0;
}