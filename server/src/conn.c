#include <conn.h>

#include <server.h>
#include <signal.h>

#define OPENCONN 1
#define CLOSECONN 2
#define OPENFILE 3
#define READFILE 4
#define WRITEFILE 5
#define APPENDTO 6
#define LOCKFILE 7
#define UNLOCKFILE 8
#define CLOSEFILE 9
#define REMOVEFILE 10

#define NO_FLAGS 0
#define O_CREAT 1
#define O_LOCK 2
#define BOTH_FLAGS 3

#define NUM_LEN 10

#define SZCHAR sizeof(char)
int getRequest(int fd, Request **req)
{
    errno = 0;

    ec_z(*req = malloc(sizeof(Request)), return -1);
    char op, oflags, nfiles, pathLen, appendLen, dirnameLen;
    // char per leggere un intero di 1 byte

    // Leggeremo:
    // 1B_op;1B_oflag;1B_nfiles;1B_pathLen;1B_appendLen;1B_dirnameLen;\
    // pathLen_path;appendLen_append;dirnameLen_dirname

    // LETTURA FLAGS e LUNGHEZZE

    // sizeof(char) dovrebbe essere sempre 1, ma lasciamo sizeof(char)
    // NB: se la read fallisce, setta errno e lo gestirà il chiamante
    ec_n(readn(fd, &op, SZCHAR), SZCHAR, free(*req); return -1;); // should read 1 byte
    ec_n(readn(fd, &oflags, SZCHAR), SZCHAR, free(*req); return -1;);
    ec_n(readn(fd, &nfiles, SZCHAR), SZCHAR, free(*req); return -1;);
    ec_n(readn(fd, &pathLen, SZCHAR), SZCHAR, free(*req); return -1;);
    ec_n(readn(fd, &appendLen, SZCHAR), SZCHAR, free(*req); return -1;);
    ec_n(readn(fd, &dirnameLen, SZCHAR), SZCHAR, free(*req); return -1;);

    // LETTURA CAMPI "DINAMICI"
    // TODO check +1;
    // PATH
    ec_z((*req)->path = calloc(pathLen + 1, SZCHAR), free(*req); return -1;);
    ec_n(readn(fd, (*req)->path, pathLen * SZCHAR), pathLen * SZCHAR, return -1;);
    // APPEND
    ec_z((*req)->append = calloc(appendLen + 1, SZCHAR), free((*req)->path); free(*req); return -1;);
    ec_n(readn(fd, (*req)->append, appendLen * SZCHAR), appendLen * SZCHAR, return -1;);
    // DIRNAME
    ec_z((*req)->dirname = calloc(dirnameLen + 1, SZCHAR), free((*req)->path); free((*req)->append); free(req); return -1;);
    ec_n(readn(fd, (*req)->dirname, dirnameLen * SZCHAR), dirnameLen * SZCHAR, return -1;);

    // Conversione a size_t
    // TODO è brutto castare implicitamente un char a size_t?
    (*req)->op = op;
    (*req)->flags = oflags;
    (*req)->nfiles = nfiles;
    (*req)->pathLen = pathLen;
    (*req)->appendLen = appendLen;
    (*req)->dirnameLen = dirnameLen;

    // TODO add client structure
    return 0; // success
}

sigset_t initSigMask()
{
    sigset_t set;
    /*we want to handle these signals, we will do it with sigwait*/
    ec(sigemptyset(&set), -1, exit(EXIT_FAILURE));        /*empty mask*/
    ec(sigaddset(&set, SIGINT), -1, exit(EXIT_FAILURE));  /* it will be handled with sigwait only */
    ec(sigaddset(&set, SIGQUIT), -1, exit(EXIT_FAILURE)); /* it will be handled with sigwait only */
    ec(sigaddset(&set, SIGTSTP), -1, exit(EXIT_FAILURE)); /* it will be handled with sigwait only */
    ec(sigaddset(&set, SIGHUP), -1, exit(EXIT_FAILURE));  /* it will be handled with sigwait only */
    ec(pthread_sigmask(SIG_SETMASK, &set, NULL), -1, exit(EXIT_FAILURE));
    return set;
}