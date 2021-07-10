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

#define ec_n_EOF(s, r, c)                                   \
    do                                                      \
    {                                                       \
        if ((s) != (r))                                     \
        {                                                   \
            if (errno == ECONNRESET || errno == ENOTCONN)   \
                puts("\tServer went down. Disconnecting..."); \
            else                                            \
                perror(#s);                                 \
            c;                                              \
        }                                                   \
    } while (0);

#define ec_n_PIPE(s, r, c)                                   \
    do                                                      \
    {                                                       \
        if ((s) != (r))                                     \
        {                                                   \
            if (errno == EPIPE)   \
                puts("\tServer went down. Disconnecting..."); \
            else                                            \
                perror(#s);                                 \
            c;                                              \
        }                                                   \
    } while (0);

char skname[PATH_MAX] = ""; // active connection
int skfd = 0;

evictedFile *readEvicted();
int readFromDisk(const char *path, void **toRet, size_t *size);
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
    interval.tv_nsec = (msec % 1000) * 1000000;

    ec_neg1(skfd = socket(AF_UNIX, SOCK_STREAM, 0), return -1);

    while (connect(skfd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    {
        time_t curr = time(NULL); // time_t equals long
        ec_z(curr, return .1);
        p(printf("Socket not found. Trying again in %dmsec\n", msec));
        if (curr > abstime.tv_sec)
        {
            errno = ETIME;
            return -1; // time expired
        }
        ec_neg1(nanosleep(&interval, NULL), return -1);
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
 * (If set) stores the evicted file (if any) in dirEvicted
 * @param flags 0 NONE, 1 O_CREAT, 2 O_LOCK, 3 BOTH_FLAGS
 * @returns 0 success, -1 failure
 */
int openFile(const char *pathname, int flags)
{
    if ((flags < 0 || flags > 3) || !pathname)
    {
        p(puts(BRED "Invalid argument" REG))
            errno = EINVAL;
        return -1;
    }
    char *path = NULL;
    ec_z(path = getAbsolutePath(pathname), return -1);
    p(printf("openFile__%s__%d: ", path, flags));
    // SEND REQUEST
    unsigned short pathLen = strnlen(path, PATH_MAX);
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    const char cflags = flags;
    char *req;
    ec_z(req = genRequest(reqLen, OPEN_FILE, cflags, 0, pathLen, 0, 0, path, "", ""), free(path); return -1);
    free(path);
    ec_n_PIPE(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    size_t nEvicted = 0;
    ec_n_EOF(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);
    ec_n_EOF(readn(skfd, &nEvicted, sizeof(size_t)), sizeof(size_t), return -1);

    p(puts(strerror(res)));

    if (nEvicted)
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvictedPath(fptr), freeEvicted(fptr); return -1););
        if (dirEvicted)
        {
            storeFileInDir(fptr, dirEvicted);
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
    if (!pathname)
    {
        p(puts(BRED "Invalid argument" REG))
            errno = EINVAL;
        return -1;
    }
    char *path = NULL;
    ec_z(path = getAbsolutePath(pathname), return -1);
    p(printf("readFile__%s: ", path));
    // SEND REQUEST
    unsigned short pathLen = strnlen(path, PATH_MAX);
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, READ_FILE, 0, 0, pathLen, 0, 0, path, "", ""), free(path); return -1);
    free(path);
    ec_n_PIPE(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    ec_n_EOF(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);
    p(puts(strerror(res)));

    if (res == SUCCESS) // SUCCESS
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvictedPath(fptr), freeEvicted(fptr); return -1));

        // copy evicted's content in buf
        ec_z(*buf = malloc(sizeof(char) * fptr->size), freeEvicted(fptr); return -1);
        memcpy(*buf, fptr->content, fptr->size);
        *size = fptr->size;

        freeEvicted(fptr);
    }
    errno = res ? res : errno;
    return res ? -1 : 0;
}

/**
 * @param N if <= 0 reads every (unlocked or owned) file in the storage
 * @param dirname where to store received files, must not be NULL
 * @returns #filesRead success, -1 failure
 */
int readNFiles(int N, const char *dirname)
{
    if (!dirname)
    {
        p(puts(BRED "Invalid argument" REG))
            errno = EINVAL;
        return -1;
    }
    p(printf("readNFiles__%d__%s: ", N, dirname));
    // SEND REQUEST
    unsigned short dirnameLen = strnlen(dirname, PATH_MAX);
    size_t reqLen = REQ_LEN_SIZE + dirnameLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, READN_FILES, 0, N, 0, dirnameLen, 0, "", dirname, ""), return -1);
    ec_n_PIPE(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    size_t nread;
    ec_n_EOF(readn(skfd, &nread, sizeof(size_t)), sizeof(size_t), return -1);
    p(puts(strerror(0)));

    // note: if the 'server-sided' readNfiles fails, nread = 0
    int i_nread = nread;
    while (nread--) // SUCCESS
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvictedPath(fptr), freeEvicted(fptr); return -1));

        storeFileInDir(fptr, dirname);
        p(printf("File stored in %s\n", dirname));

        freeEvicted(fptr);
    }
    return i_nread;
}

/**
 * @param pathname file to be read from disk and to be sent to the server
 * @param dirname if != NULL, store evicted files here, if any
 */
int writeFile(const char *pathname, const char *dirname)
{
    if (!pathname)
    {
        p(puts(BRED "Invalid argument" REG))
            errno = EINVAL;
        return -1;
    }
    char *path = NULL;
    ec_z(path = getAbsolutePath(pathname), return -1);
    p(printf("writeFile__%s__%s: ", path, dirname));

    // READ FROM DISK
    void *buf = NULL;
    size_t size = 0;
    ec_neg1(readFromDisk(pathname, &buf, &size), return -1);

    // SEND REQUEST
    unsigned short pathLen = strnlen(path, PATH_MAX);
    unsigned short dirnameLen = 0;
    dirnameLen = dirname ? strnlen(dirname, PATH_MAX) : 0;
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char) + dirnameLen * sizeof(char) + size;
    char *req;

    ec_z(req = genRequest(reqLen, WRITE_FILE, 0, 0, pathLen, dirnameLen, size, path, dirname, buf), free(buf); free(path); return -1);
    free(path);
    ec_n_PIPE(writen(skfd, req, reqLen), reqLen, free(req); free(buf); return -1);
    free(req);
    free(buf);

    // GET RESPONSE
    int res = 0;
    size_t nEvicted = 0;
    ec_n_EOF(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);
    ec_n_EOF(readn(skfd, &nEvicted, sizeof(size_t)), sizeof(size_t), return -1);
    p(puts(strerror(res)));

    while (nEvicted--)
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvictedPath(fptr), freeEvicted(fptr); return -1));
        if (dirname)
        {
            storeFileInDir(fptr, dirname);
            p(printf("File stored in %s\n", dirname));
        }
        freeEvicted(fptr);
    }
    errno = res ? res : errno;
    return res ? -1 : 0;
}

/**
 * Note: doesn't free buf
 * @param buf content to append
 * @param size buf's size
 * @param dirname if != NULL, store evicted files here, if any
 */
int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname)
{
    if (!pathname || !buf)
    {
        p(puts(BRED "Invalid argument" REG))
            errno = EINVAL;
        return -1;
    }
    char *path = NULL;
    ec_z(path = getAbsolutePath(pathname), return -1);
    p(printf("appendToFile__%s__%ld__%s: ", path, size, dirname));
    // SEND REQUEST
    unsigned short pathLen = strnlen(path, PATH_MAX);
    unsigned short dirnameLen = 0;
    dirnameLen = dirname ? strnlen(dirname, PATH_MAX) : 0;
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char) + dirnameLen * sizeof(char) + size;
    char *req;
    ec_z(req = genRequest(reqLen, APPEND, 0, 0, pathLen, dirnameLen, size, path, dirname, buf), free(path); return -1);
    free(path);
    ec_n_PIPE(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    size_t nEvicted = 0;
    ec_n_EOF(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);
    ec_n_EOF(readn(skfd, &nEvicted, sizeof(size_t)), sizeof(size_t), return -1);
    p(puts(strerror(res)));

    while (nEvicted--)
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvictedPath(fptr), freeEvicted(fptr); return -1));
        if (dirname)
        {
            storeFileInDir(fptr, dirname);
            p(printf("File stored in %s\n", dirname));
        }
        freeEvicted(fptr);
    }
    errno = res ? res : errno;
    return res ? -1 : 0;
}
/**
 * Tries to set O_LOCK.
 * Doesn't return until O_LOCK is acquired or an error is received 
 * @returns 0 success, -1 error
 */
int lockFile(const char *pathname)
{
    if (!pathname)
    {
        p(puts(BRED "Invalid argument" REG))
            errno = EINVAL;
        return -1;
    }
    char *path = NULL;
    ec_z(path = getAbsolutePath(pathname), return -1);
    p(printf("lockFile__%s: ", path));
    // SEND REQUEST
    unsigned short pathLen = strnlen(path, PATH_MAX);
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, LOCK_FILE, 0, 0, pathLen, 0, 0, path, "", ""), free(path); return -1);
    free(path);
    ec_n_PIPE(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    ec_n_EOF(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);

    p(puts(strerror(res)));

    errno = res ? res : errno;
    return res ? -1 : 0;
}

/**
 * Removes O_LOCK from a file.
 * @returns 0 success, -1 error
 */
int unlockFile(const char *pathname)
{
    if (!pathname)
    {
        p(puts(BRED "Invalid argument" REG))
            errno = EINVAL;
        return -1;
    }
    char *path = NULL;
    ec_z(path = getAbsolutePath(pathname), return -1);
    p(printf("unlockFile__%s: ", path));
    // SEND REQUEST
    unsigned short pathLen = strnlen(path, PATH_MAX);
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, UNLOCK_FILE, 0, 0, pathLen, 0, 0, path, "", ""), free(path); return -1);
    free(path);
    ec_n_PIPE(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    ec_n_EOF(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);

    p(puts(strerror(res)));

    errno = res ? res : errno;
    return res ? -1 : 0;
}

/**
 * Closes the given file in the storage
 */
int closeFile(const char *pathname)
{
    if (!pathname)
    {
        p(puts(BRED "Invalid argument" REG))
            errno = EINVAL;
        return -1;
    }
    char *path = NULL;
    ec_z(path = getAbsolutePath(pathname), return -1);
    p(printf("closeFile__%s: ", path));
    // SEND REQUEST
    unsigned short pathLen = strnlen(path, PATH_MAX);
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, CLOSE_FILE, 0, 0, pathLen, 0, 0, path, "", ""), free(path); return -1);
    free(path);
    ec_n_PIPE(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    ec_n_EOF(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);

    p(puts(strerror(res)));

    errno = res ? res : errno;
    return res ? -1 : 0;
}
/**
 * Removes file from the server and (if set) stores it in dirEvicted.
 * @returns 0 success, -1 error
 */
int removeFile(const char *pathname)
{
    if (!pathname)
    {
        p(puts(BRED "Invalid argument" REG))
            errno = EINVAL;
        return -1;
    }
    char *path = NULL;
    ec_z(path = getAbsolutePath(pathname), return -1);
    p(printf("removeFile__%s: ", path));
    // SEND REQUEST
    unsigned short pathLen = strnlen(path, PATH_MAX);
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, REMOVE_FILE, 0, 0, pathLen, 0, 0, path, "", ""), free(path); return -1);
    free(path);
    ec_n_PIPE(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    int res = 0;
    ec_n_EOF(readn(skfd, &res, sizeof(int)), sizeof(int), return -1);

    p(puts(strerror(res)));

    if (res == 0)
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvictedPath(fptr), freeEvicted(fptr); return -1););
        if (dirEvicted)
        {
            storeFileInDir(fptr, dirEvicted);
            p(printf("File stored in %s\n", dirEvicted));
        }
        freeEvicted(fptr);
        p(printf("\tFile trashed\n"));
    }

    errno = res ? res : errno;
    return res ? -1 : 0;
}

/**
 * @returns Request on success, NULL on error
 */
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
/**
 * (server might shutdown while reading)
 * @returns evictedFile on success, NULL on error
 */
evictedFile *readEvicted()
{
    evictedFile *f;
    ec_z(f = malloc(sizeof(evictedFile)), return NULL);
    f->path = NULL;
    f->content = NULL;
    size_t pathLen;
    ec_n_EOF(readn(skfd, &pathLen, sizeof(size_t)), sizeof(size_t), freeEvicted(f); return NULL);
    ec_z(f->path = calloc(pathLen + 1, sizeof(char)), freeEvicted(f); return NULL);
    ec_n_EOF(readn(skfd, f->path, pathLen), pathLen, freeEvicted(f); return NULL);
    ec_n_EOF(readn(skfd, &f->size, sizeof(size_t)), sizeof(size_t), freeEvicted(f); return NULL);
    ec_z(f->content = calloc(f->size, sizeof(char)), freeEvicted(f); return NULL);
    ec_n_EOF(readn(skfd, f->content, f->size), f->size, freeEvicted(f); return NULL);
    return f;
}
