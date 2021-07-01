#include <conn.h>

void setFlags(Request *req, int flags);

#define SZSHORT sizeof(unsigned short)
#define SZINT sizeof(int)

Request *getRequest(int fd, int *msg)
{
    errno = 0;
    Request *req;
    ec_z(req = malloc(sizeof(Request)), return NULL);
    char oflags,fdBuf[INT_LEN];
    req->path = NULL;
    req->append = NULL;
    req->dirname = NULL;
    // char per leggere un intero di 1 byte
    //    Request structure: (without ';')
    //   {1B_op;1B_oflag;4B_nfiles;2B_pathLen;2_dirnameLen;10B_appendLen;
    //    pathLen_path;appendLen_append;dirnameLen_dirname}

    // LETTURA FLAGS e LUNGHEZZE

    // sizeof(char) dovrebbe essere sempre 1, ma lasciamo sizeof(char)
    // NB: se la read fallisce, setta errno e lo gestirà il chiamante
    ec_neg1(readn(fd, &(req->op), SZCHAR), free(req); return NULL;); // should read 1 byte
    if (req->op == 0)                                                   // client closed the connection
    {
        *msg = 1;
        free(req);
        return NULL;
    }

    ec_neg1(readn(fd, &(oflags), SZCHAR), free(req); return NULL;);
    ec_neg1(readn(fd, &(req->nfiles), SZINT), free(req); return NULL;);
    ec_neg1(readn(fd, &(req->pathLen), SZSHORT), free(req); return NULL;);
    ec_neg1(readn(fd, &(req->dirnameLen), SZSHORT), free(req); return NULL;);
    ec_neg1(readn(fd, &(req->appendLen), sizeof(size_t)), free(req); return NULL;);

    // LETTURA CAMPI "DINAMICI"
    // TODO check +1;
    // PATH
    ec_z(req->path = calloc(req->pathLen + 1, SZCHAR), freeRequest(req); return NULL;);
    ec_neg1(readn(fd, req->path, req->pathLen * SZCHAR), return NULL;);
    // DIRNAME
    ec_z(req->dirname = calloc(req->dirnameLen + 1, SZCHAR), freeRequest(req); return NULL;);
    ec_neg1(readn(fd, req->dirname, req->dirnameLen * SZCHAR), return NULL;);
    // APPEND
    ec_z(req->append = calloc(req->appendLen + 1, SZCHAR), freeRequest(req); return NULL;);
    ec_neg1(readn(fd, req->append, req->appendLen * SZCHAR), return NULL;);

    // Conversione a unsigned short
    // TODO è brutto castare implicitamente un char a unsigned short?
    setFlags(req, oflags);

    // Only the thread dispatcher is allowed to add clients
    ec_neg1(snprintf(fdBuf, INT_LEN, "%06d", fd), freeRequest(req); return NULL;);
    ec_z(req->client = icl_hash_find(clients, fdBuf), freeRequest(req); errno = EBADF; return NULL;);

    return req;
}

void setFlags(Request *req, int flags)
{
    req->o_creat = 0;
    req->o_lock = 0;
    switch (flags)
    {
    case ONLY_CREAT:
        req->o_creat = 1;
        break;
    case ONLY_LOCK:
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
    /*we want to handle these signals, we will do it with sigwait*/
    ec(sigemptyset(&set), -1, exit(EXIT_FAILURE));        /*empty mask*/
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
    ec_nz(icl_hash_find(clients, fdBuf), free(newClient); errno = EADDRINUSE; return NULL);
    ec_z(fdTmp = strndup(fdBuf, INT_LEN), return NULL);
    // ec_z(fdTmp = malloc(INT_LEN * sizeof(char) + 1), free(newClient); return NULL);
    // strncpy(fdTmp,fdBuf,INT_LEN);
    icl_hash_insert(clients, fdTmp, newClient);
    return newClient;
}

_Bool NoMoreClients()
{
    if (clients->nentries)
        return 0;
    return 1;
}

// debug utility
void printRequest(Request *req)
{
    printf("REQ: %d %d %d %d %hd %hd %ld %s %s %s\n",
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