#include <conn.h>

void setFlags(Request *req, int flags);

#define SZSHORT sizeof(unsigned short)
#define SZINT sizeof(int)

#define ec_n_EOF(s, r, c)          \
    do                             \
    {                              \
        if ((s) != (r))            \
        {                          \
            if (errno != ENOTCONN) \
                perror(#s);        \
            c;                     \
        }                          \
    } while (0);

// TODO remmove printf debug
/**
 * Reads a request from an fd.
 * errno = ENOTCONN if reads eof from client
 * @returns Request* success, NULL on error or EOF (errno set)
 */
Request *getRequest(int fd)
{
    errno = 0;
    Request *req;
    ec_z(req = malloc(sizeof(Request)), return NULL);
    char oflags, fdBuf[INT_LEN];
    req->path = NULL;
    req->append = NULL;
    req->dirname = NULL;
    //    Request structure: (without ';')
    //   {1B_op;1B_oflag;4B_nfiles;2B_pathLen;2_dirnameLen;10B_appendLen;
    //    pathLen_path;appendLen_append;dirnameLen_dirname}

    // sizeof(char) dovrebbe essere sempre 1, ma lasciamo sizeof(char)
    // NB: se la read fallisce, setta errno e lo gestirà il chiamante

    int bread = 0, singleRead;
    ec_n_EOF(singleRead = readn(fd, &(req->op), SZCHAR), SZCHAR, free(req); return NULL;); // should read 1 byte
    // printf("%d\n", singleRead);
    bread += singleRead;

    ec_n_EOF(singleRead = readn(fd, &(oflags), SZCHAR), SZCHAR, free(req); return NULL;);
    // printf("%d\n", singleRead);
    bread += singleRead;
    ec_n_EOF(singleRead = readn(fd, &(req->nfiles), SZINT), SZINT, free(req); return NULL;);
    // printf("%d\n", singleRead);
    bread += singleRead;
    ec_n_EOF(singleRead = readn(fd, &(req->pathLen), SZSHORT), SZSHORT, free(req); return NULL;);
    // printf("%d\n", singleRead);
    bread += singleRead;
    ec_n_EOF(singleRead = readn(fd, &(req->dirnameLen), SZSHORT), SZSHORT, free(req); return NULL;);
    // printf("%d\n", singleRead);
    bread += singleRead;
    ec_n_EOF(singleRead = readn(fd, &(req->appendLen), sizeof(size_t)), sizeof(size_t), free(req); return NULL;);
    // printf("%d\n", singleRead);
    bread += singleRead;

    // LETTURA CAMPI "DINAMICI"
    // PATH
    int pathlen;
    ec_z(req->path = calloc(req->pathLen + 1, SZCHAR), freeRequest(req); return NULL;);
    ec_n_EOF(pathlen = readn(fd, req->path, req->pathLen * SZCHAR), req->pathLen, freeRequest(req); return NULL;); // DIRNAME
    // printf("path: %d\n", pathlen);
    bread += pathlen;
    // if (req->dirnameLen)
    {
        ec_z(req->dirname = calloc(req->dirnameLen + 1, SZCHAR), freeRequest(req); return NULL;);

        ec_n_EOF(singleRead = readn(fd, req->dirname, req->dirnameLen * SZCHAR), req->dirnameLen, freeRequest(req); return NULL;);
        // printf("%d\n", singleRead);
        bread += singleRead;
    }
    // APPEND
    // if (req->appendLen)
    {
        ec_z(req->append = calloc(req->appendLen + 1, SZCHAR), freeRequest(req); return NULL;);

        ec_n_EOF(singleRead = readn(fd, req->append, req->appendLen * SZCHAR), req->appendLen, freeRequest(req); return NULL;);
        // printf("%d\n", singleRead);
        bread += singleRead;
    }

    // Conversione a unsigned short
    setFlags(req, oflags);

    // Add client
    ec_neg(snprintf(fdBuf, INT_LEN, "%06d", fd), freeRequest(req); return NULL;);
    ec_nz(LOCKCLIENTS, freeRequest(req); return NULL);
    ec_z(req->client = icl_hash_find(clients, fdBuf), freeRequest(req); errno = EBADF; return NULL;);
    ec_nz(UNLOCKCLIENTS, freeRequest(req); return NULL);
    // printf("bytes read: %d\n", bread);
    return req;
}

void setFlags(Request *req, int flags)
{
    req->o_creat = 0;
    req->o_lock = 0;
    switch (flags)
    {
    case _O_CREAT:
        req->o_creat = 1;
        break;
    case _O_LOCK:
        req->o_lock = 1;
        break;
    case BOTH_FLAGS:
        req->o_lock = 1;
        req->o_creat = 1;
    default:
        break;
    }
    return;
}
/**
 * Masks SIGINT,SIGQUIT and SIGHUP
 * @returns set, dies on error
 */
sigset_t initSigMask()
{
    sigset_t set;

    // TODO clean
    // mask all signals
    // ec_neg1(sigfillset(&set), exit(EXIT_FAILURE));
    // ec_neg1(pthread_sigmask(SIG_SETMASK, &set, NULL), exit(EXIT_FAILURE)); 

        // ignore SIGPIPE
        struct sigaction saa;
    memset(&saa, 0, sizeof(saa));
    saa.sa_handler = SIG_IGN;
    ec_neg1(sigaction(SIGPIPE, &saa, NULL), exit(EXIT_FAILURE));

        /*we want to handle these signals, we will do it with sigwait*/
        ec(sigemptyset(&set), -1, exit(EXIT_FAILURE));    /*empty mask*/
    ec(sigaddset(&set, SIGINT), -1, exit(EXIT_FAILURE));  /* it will be handled with sigwait only */
    ec(sigaddset(&set, SIGQUIT), -1, exit(EXIT_FAILURE)); /* it will be handled with sigwait only */
    ec(sigaddset(&set, SIGHUP), -1, exit(EXIT_FAILURE));  /* it will be handled with sigwait only */
    ec(pthread_sigmask(SIG_BLOCK, &set, NULL), -1, exit(EXIT_FAILURE));
    return set;
}

/**
 * Frees a request, but doesn't free Request->client
 */
void freeRequest(void *arg)
{
    Request *req = arg;
    free(req->path);
    free(req->append);
    free(req->dirname);
    free(req);
    return;
}

/**
 * Alloc a Client structure from an fd and adds it to the 'clients' hash_t
 * @returns new Client, NULL on failure (EADDRINUSE | ENOMEM);
 */
Client *addClient(int fd)
{
    Client *newClient = NULL;
    char fdBuf[INT_LEN], *fdTmp;
    ec_z(newClient = malloc(sizeof(Client)), return NULL;);
    newClient->fd = fd;
    if (snprintf(fdBuf, INT_LEN, "%06d", fd) < 0)
        return NULL;
    ec_nz(LOCKCLIENTS, free(newClient); return NULL);

    ec_nz(icl_hash_find(clients, fdBuf), free(newClient); UNLOCKCLIENTS; errno = EADDRINUSE; return NULL);
    ec_z(fdTmp = strndup(fdBuf, INT_LEN), return NULL);
    icl_hash_insert(clients, fdTmp, newClient);

    ec_nz(UNLOCKCLIENTS, free(newClient); return NULL);
    return newClient;
}

_Bool NoMoreClients()
{
    ec_nz(LOCKCLIENTS, return 0;);
    if (clients->nentries)
    {
        ec_nz(UNLOCKCLIENTS, return 0;);
        return 0;
    }
    ec_nz(UNLOCKCLIENTS, return 0;);
    return 1;
}

// debug utility
void printRequest(Request *req, int fd)
{
    printf("REQ %d: %d %d %d %d %hd %hd %ld %s %s %s\n",
           fd,
           req->op,
           req->o_creat,
           req->o_lock,
           req->nfiles,
           req->pathLen,
           req->dirnameLen,
           req->appendLen,
           req->path,
           req->dirname,
           req->append);
    return;
}