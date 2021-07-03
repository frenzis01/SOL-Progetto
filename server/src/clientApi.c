#include <clientApi.h>

#define p(c)   \
    if (pFlag) \
    {          \
        c;     \
    }
#define CHK_SK                                                 \
    if (!sockname || strncmp(skname, sockname, UNIX_PATH_MAX)) \
    {                                                          \
        errno = EINVAL;                                        \
        return -1;                                             \
    }

typedef struct
{
    char
        *path,
        *content;
    size_t pathLen, size;
} evictedFile;

int printString(const char *str, size_t len)
{
    for (size_t i = 0; i < len; i++)
        ec_neg(printf("%c", str[i]), return -1);

    return 0;
}
int printEvicted(void *arg)
{
    evictedFile *c = arg;
    ec_neg(printf(ANSI_COLOR_MAGENTA "EVCTD -- PATH: %s | CONTENT: ", c->path), return -1);
    ec_neg(printString(c->content, c->size), return -1);
    ec_neg(printf(ANSI_COLOR_RESET "\n"), return -1);
    errno = 0;
    return 0;
}

char skname[UNIX_PATH_MAX] = ""; // active connection
int skfd = 0;
_Bool pFlag = 1;
char *dirEvicted = NULL;

evictedFile *readEvicted();
void freeEvicted(void *arg);
char *genRequest(size_t reqLen,
                 char op,
                 char flags,
                 int nfiles,
                 unsigned short pathLen,
                 unsigned short dirnameLen,
                 size_t appendLen,
                 const char *path,
                 const char *append,
                 const char *dirname);

/**
 * Si tenta di aprire una connessione al socket 'sockname' ogni 'msec' ms,
 * fino allo scadere del tempo 'abstime'
 * @param sockname name of the socket
 * @param msec time interval between one connection try and another
 * @param abstime time to elapse before unsuccessfully returning
 * @returns 0 success, -1 error
 */
int openConnection(const char *sockname, int msec, const struct timespec abstime)
{
    struct sockaddr_un sa;
    strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;
    struct timespec interval;
    interval.tv_sec = msec / 1000;
    interval.tv_nsec = 0;

    ec_neg1(skfd = socket(AF_UNIX, SOCK_STREAM, 0), return -1);

    while (connect(skfd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        time_t curr = time(NULL); // time_t equals long
        p(printf("Socket not found. Trying again in %dmsec\n", msec));
        if (curr > abstime.tv_sec)
        {
            errno = ETIME;
            return -1; // time expired
        }
        nanosleep(&interval, NULL);
    }
    strncpy(skname, sockname, UNIX_PATH_MAX);
    return 0;
}

/**
 * @returns 0 success, -1 failure
 */
int closeConnection(const char *sockname)
{
    CHK_SK;
    return close(skfd);
}

/**
 * @param flags 0 NONE, 1 O_CREAT, 2 O_LOCK, 3 BOTH_FLAGS
 * @returns 0 success, -1 failure
 */
int openFile(const char *pathname, int flags)
{
    p(printf("openFile__%s__%d: ", pathname, flags));
    if ((flags < 0 || flags > 3) || !pathname)
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
    // TODO note, without +1 doesn't work
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    const char cflags = flags;
    char *req;
    ec_z(req = genRequest(reqLen, OPEN_FILE, cflags, 0, pathLen, 0, 0, pathname, "", ""), return -1);
    ec_n(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    size_t nEvicted = 0;
    ec_n(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);
    ec_n(readn(skfd, &nEvicted, sizeof(size_t)), sizeof(size_t), return -1);

    p(puts(strerror(res)));

    if (nEvicted)
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvicted(fptr), freeEvicted(fptr); return -1););
        if (dirEvicted)
        {
            // TODO store file
            p(printf("File stored in %s\n", dirEvicted));
        }
        freeEvicted(fptr);
        p(printf("File trashed\n"));
    }
    errno = res ? res : errno;
    return res ? -1 : 0;
}

/**
 * @param buf where to store the read file's content
 * @param size content's size
 */
int readFile(const char *pathname, void **buf, size_t *size)
{
    p(printf("readFile__%s: ", pathname));
    if (!pathname)
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, READ_FILE, 0, 0, pathLen, 0, 0, pathname, "", ""), return -1);
    ec_n(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    ec_n(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);
    p(puts(strerror(res)));

    if (res == SUCCESS) // SUCCESS
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvicted(fptr), freeEvicted(fptr); return -1));

        // copy evicted's content in buf
        ec_z(*buf = malloc(sizeof(char) * fptr->size), freeEvicted(fptr); return -1);
        memcpy(*buf, fptr->content, fptr->size);
        *size = fptr->size;

        freeEvicted(fptr);
    }
    errno = res ? res : errno;
    return res ? -1 : 0;
}
int readNFiles(int N, const char *dirname)
{
    p(printf("readNFiles__%d__%s: ", N, dirname));
    if (!dirname)
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    // SEND REQUEST
    unsigned short dirnameLen = strnlen(dirname, PATH_MAX);
    size_t reqLen = REQ_LEN_SIZE + dirnameLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, READN_FILES, 0, N, 0, dirnameLen, 0, "", dirname, ""), return -1);
    ec_n(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    size_t nread;
    // ec_n(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);
    ec_n(readn(skfd, &nread, sizeof(size_t)), sizeof(size_t), return -1);
    p(puts(strerror(res)));

    // note: if the 'server-sided' readNfiles fails, no
    // printf("nread = %ld\n", nread);
    while (nread--) // SUCCESS
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvicted(fptr), freeEvicted(fptr); return -1));

        // TODO save in dirname

        freeEvicted(fptr);
    }
    errno = res ? res : errno;
    return res ? -1 : 0;
}
int writeFile(const char *pathname, const char *dirname);
/**
 * @param buf content to append
 * @param size buf's size
 * @param dirname if != NULL, store evicted files here, if any
 */
int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname)
{
    p(printf("appendToFile__%s__%ld__%s: ", pathname, size, dirname));
    if (!pathname || !buf)
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
    unsigned short dirnameLen = 0;
    // dirname ? dirnameLen=strnlen(dirname, PATH_MAX) : dirname;
    dirnameLen = dirname ? strnlen(dirname, PATH_MAX) : 0;
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char) + dirnameLen * sizeof(char) + size;
    char *req;
    ec_z(req = genRequest(reqLen, APPEND, 0, 0, pathLen, dirnameLen, size, pathname, dirname, buf), return -1);
    ec_n(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    size_t nEvicted = 0;
    ec_n(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);
    ec_n(readn(skfd, &nEvicted, sizeof(size_t)), sizeof(size_t), return -1);
    p(puts(strerror(res)));

    while (nEvicted--)
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvicted(fptr), freeEvicted(fptr); return -1));
        // TODO store to dirname
        freeEvicted(fptr);
    }
    errno = res ? res : errno;
    return res ? -1 : 0;
}
int lockFile(const char *pathname)
{
    p(printf("lockFile__%s: ", pathname));
    if (!pathname)
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
    // TODO note, without +1 doesn't work
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, LOCK_FILE, 0, 0, pathLen, 0, 0, pathname, "", ""), return -1);
    ec_n(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    ec_n(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);

    p(puts(strerror(res)));

    errno = res ? res : errno;
    return res ? -1 : 0;
}
int unlockFile(const char *pathname)
{
    p(printf("unlockFile__%s: ", pathname));
    if (!pathname)
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
    // TODO note, without +1 doesn't work
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, UNLOCK_FILE, 0, 0, pathLen, 0, 0, pathname, "", ""), return -1);
    ec_n(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    ec_n(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);

    p(puts(strerror(res)));

    errno = res ? res : errno;
    return res ? -1 : 0;
}
int closeFile(const char *pathname)
{
    p(printf("closeFile__%s: ", pathname));
    if (!pathname)
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
    // TODO note, without +1 doesn't work
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, CLOSE_FILE, 0, 0, pathLen, 0, 0, pathname, "", ""), return -1);
    ec_n(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    ec_n(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);

    p(puts(strerror(res)));

    errno = res ? res : errno;
    return res ? -1 : 0;
}
int removeFile(const char *pathname)
{
    p(printf("removeFile__%s: ", pathname));
    if (!pathname)
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
    // TODO note, without +1 doesn't work
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, REMOVE_FILE, 0, 0, pathLen, 0, 0, pathname, "", ""), return -1);
    ec_n(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    ec_n(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);

    p(puts(strerror(res)));

    if (res == 0)
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvicted(fptr), freeEvicted(fptr); return -1););
        if (dirEvicted)
        {
            // TODO store file
            p(printf("File stored in %s\n", dirEvicted));
        }
        freeEvicted(fptr);
        p(printf("File trashed\n"));
    }

    errno = res ? res : errno;
    return res ? -1 : 0;
}

char *genRequest(size_t reqLen,
                 char op,
                 char flags,
                 int nfiles,
                 unsigned short pathLen,
                 unsigned short dirnameLen,
                 size_t appendLen,
                 const char *path,
                 const char *dirname,
                 const char *append)
{
    size_t index = 0;
    char *buf;
    ec_z(buf = calloc(reqLen, sizeof(char)), return NULL;);

    memcpy(buf + index, &op, sizeof(char));
    index += sizeof(char);
    memcpy(buf + index, &flags, sizeof(char));
    index += sizeof(char);
    memcpy(buf + index, &nfiles, sizeof(int));
    index += sizeof(int);
    memcpy(buf + index, &pathLen, sizeof(unsigned short));
    index += sizeof(unsigned short);
    memcpy(buf + index, &dirnameLen, sizeof(unsigned short));
    index += sizeof(unsigned short);
    memcpy(buf + index, &appendLen, sizeof(size_t));
    index += sizeof(size_t);
    memcpy(buf + index, path, pathLen * sizeof(char));
    index += pathLen * sizeof(char);
    memcpy(buf + index, dirname, dirnameLen * sizeof(char));
    index += dirnameLen * sizeof(char);
    memcpy(buf + index, append, appendLen);

    return buf;
}

evictedFile *readEvicted()
{
    evictedFile *f;
    ec_z(f = malloc(sizeof(evictedFile)), return NULL);
    f->path = NULL;
    f->content = NULL;
    ec_n(readn(skfd, &f->pathLen, sizeof(size_t)), sizeof(size_t), freeEvicted(f); return NULL);
    ec_z(f->path = calloc(f->pathLen + 1, sizeof(char)), freeEvicted(f); return NULL);
    ec_n(readn(skfd, f->path, f->pathLen), f->pathLen, freeEvicted(f); return NULL);
    ec_n(readn(skfd, &f->size, sizeof(size_t)), sizeof(size_t), freeEvicted(f); return NULL);
    ec_z(f->content = calloc(f->size, sizeof(char)), freeEvicted(f); return NULL);
    ec_n(readn(skfd, f->content, f->size), f->size, freeEvicted(f); return NULL);
    return f;
}

void freeEvicted(void *arg)
{
    evictedFile *f = arg;
    free(f->content);
    free(f->path);
    free(f);
}

/**
 * Assumes f->path is absolute
 * @returns 0 success, -1 error
 */
int storeFileInDir(evictedFile *f, const char *dirname)
{
    char
        *dir = NULL,
        *lastSlash = NULL,
        command = NULL,
        newPath = NULL;
    FILE *fptr = NULL;
    ec_z(dir = strndup(f->path, PATH_MAX), goto store_cleanup); // this includes the first '/'
    ec_z(lastSlash = strrchr(dir, '/'), goto store_cleanup);
    // if it is a valid path it has at least one '/'

    // _Bool haveToCreateDir = 1;
    // // if the last slash is the first then f is located in the root dir
    // if (lastSlash == dir)
    //     haveToCreateDir = 0;
    // if (haveToCreateDir)

    // CREATE DIRECTORY (along with parents)
    dir++; // remove the first slash

    // get updated path
    ec_z(newPath = calloc(strnlen(dir, PATH_MAX) +
                              strnlen(dirname, PATH_MAX) + 1,
                          sizeof(char)),
         goto store_cleanup);
    ec_z(snprintf(newPath, PATH_MAX, "%s%s", dir, dirname), goto store_cleanup);

    // create directory
    char sh_mkdir[] = "mkdir -p "; // -p makes parents, no error if existing
    ec_z(command = calloc(
             strlen(sh_mkdir) + strnlen(newPath, PATH_MAX) + 1,
             sizeof(char)),
         goto store_cleanup);

    ec_neg(snprintf(command, strlen(sh_mkdir) + PATH_MAX, "%s%s", sh_mkdir, newPath), goto store_cleanup);
    ec_nz(system(command), goto store_cleanup);


        // WRITE FILE
    ec_z(fptr = fopen(newPath,"w+"), goto store_cleanup);

store_cleanup:
    free(dir);
    free(command);
    free(newPath);
    return -1;
}