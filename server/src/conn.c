#include <conn.h>

void setFlags(Request *req, int flags);

Request *getRequest(int fd, int *msg)
{
    errno = 0;
    Request *req;
    ec_z(req = malloc(sizeof(Request)), return NULL);
    char op, oflags, nfiles, pathLen, appendLen, dirnameLen, fdBuf[INT_LEN];
    req->path = NULL;
    req->append = NULL;
    req->dirname = NULL;
    // char per leggere un intero di 1 byte

    // Leggeremo:
    // 1B_op;1B_oflag;1B_nfiles;1B_pathLen;1B_appendLen;1B_dirnameLen;\
    // pathLen_path;appendLen_append;dirnameLen_dirname

    // LETTURA FLAGS e LUNGHEZZE

    // sizeof(char) dovrebbe essere sempre 1, ma lasciamo sizeof(char)
    // NB: se la read fallisce, setta errno e lo gestirà il chiamante
    ec_n(readn(fd, &op, SZCHAR), SZCHAR, free(req); return NULL;); // should read 1 byte
    if (op == 0)                                                   // client closed the connection
    {
        *msg = 1;
        return NULL;
    }

    ec_n(readn(fd, &oflags, SZCHAR), SZCHAR, free(req); return NULL;);
    ec_n(readn(fd, &nfiles, SZCHAR), SZCHAR, free(req); return NULL;);
    ec_n(readn(fd, &pathLen, SZCHAR), SZCHAR, free(req); return NULL;);
    ec_n(readn(fd, &appendLen, SZCHAR), SZCHAR, free(req); return NULL;);
    ec_n(readn(fd, &dirnameLen, SZCHAR), SZCHAR, free(req); return NULL;);

    // LETTURA CAMPI "DINAMICI"
    // TODO check +1;
    // PATH
    ec_z(req->path = calloc(pathLen + 1, SZCHAR), freeRequest(req); return NULL;);
    ec_n(readn(fd, req->path, pathLen * SZCHAR), pathLen * SZCHAR, return NULL;);
    // APPEND
    ec_z(req->append = calloc(appendLen + 1, SZCHAR), freeRequest(req); return NULL;);
    ec_n(readn(fd, req->append, appendLen * SZCHAR), appendLen * SZCHAR, return NULL;);
    // DIRNAME
    ec_z(req->dirname = calloc(dirnameLen + 1, SZCHAR), freeRequest(req); return NULL;);
    ec_n(readn(fd, req->dirname, dirnameLen * SZCHAR), dirnameLen * SZCHAR, return NULL;);

    // Conversione a unsigned short
    // TODO è brutto castare implicitamente un char a unsigned short?
    setFlags(req, oflags);
    req->op = op;
    req->nfiles = nfiles;
    req->pathLen = pathLen;
    req->appendLen = appendLen;
    req->dirnameLen = dirnameLen;

    // Only the thread dispatcher is allowed to add clients
    ec_neg1(snprintf(fdBuf, INT_LEN, "%06d", fd), /* handle error */);
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
    ec_nz(newClient = malloc(sizeof(Client)), return NULL;);
    newClient->fd = fd;
    ec_neg1(snprintf(fdBuf, INT_LEN, "%06d", fd), return NULL);
    ec_nz(icl_hash_find(clients, fdBuf), free(newClient); errno = EADDRINUSE; return NULL);
    ec_z(fdTmp = strndup(fdBuf, INT_LEN), return NULL);
    icl_hash_insert(clients, fdTmp, newClient);
    return newClient;
}


_Bool NoMoreClients()
{
    if (clients->nentries)
        return 0;
    return 1;
}