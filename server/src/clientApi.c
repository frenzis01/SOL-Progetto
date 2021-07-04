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
int storeFileInDir(evictedFile *f, const char *dirname);
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
 * (If set) stores the evicted file (if any) in dirEvicted
 * @param flags 0 NONE, 1 O_CREAT, 2 O_LOCK, 3 BOTH_FLAGS
 * @returns 0 success, -1 failure
 */
int openFile(const char *pathname, int flags)
{
    if ((flags < 0 || flags > 3) || !pathname || (strchr(pathname,'/') != pathname))
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    p(printf("openFile__%s__%d: ", pathname, flags));
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
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
    if (!pathname || (strchr(pathname,'/') != pathname))
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    p(printf("readFile__%s: ", pathname));
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

/**
 * @param N if <= 0 reads every (unlocked or owned) file in the storage
 * @param dirname where to store received files, must not be NULL
 * @returns #filesRead success, -1 failure
 */
int readNFiles(int N, const char *dirname)
{
    if (!dirname)
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    p(printf("readNFiles__%d__%s: ", N, dirname));
    // SEND REQUEST
    unsigned short dirnameLen = strnlen(dirname, PATH_MAX);
    size_t reqLen = REQ_LEN_SIZE + dirnameLen * sizeof(char);
    char *req;
    ec_z(req = genRequest(reqLen, READN_FILES, 0, N, 0, dirnameLen, 0, "", dirname, ""), return -1);
    ec_n(writen(skfd, req, reqLen), reqLen, free(req); return -1);
    free(req);

    // GET RESPONSE
    size_t nread;
    ec_n(readn(skfd, &nread, sizeof(size_t)), sizeof(size_t), return -1);
    p(puts(strerror(0)));

    // note: if the 'server-sided' readNfiles fails, nread = 0
    int i_nread = nread;
    while (nread--) // SUCCESS
    {
        evictedFile *fptr = readEvicted();
        ec_z(fptr, return -1);
        p(ec_neg1(printEvicted(fptr), freeEvicted(fptr); return -1));

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
    if (!pathname || (strchr(pathname,'/') != pathname))
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    p(printf("writeFile__%s__%s: ", pathname, dirname));

    // READ FROM DISK
    void *buf = NULL;
    size_t size = 0;
    ec_neg1(readFromDisk(pathname, &buf, &size), return -1);

    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
    unsigned short dirnameLen = 0;
    dirnameLen = dirname ? strnlen(dirname, PATH_MAX) : 0;
    size_t reqLen = REQ_LEN_SIZE + pathLen * sizeof(char) + dirnameLen * sizeof(char) + size;
    char *req;

    ec_z(req = genRequest(reqLen, WRITE_FILE, 0, 0, pathLen, dirnameLen, size, pathname, dirname, buf), free(buf); return -1);
    ec_n(writen(skfd, req, reqLen), reqLen, free(req); free(buf); return -1);
    free(req);
    free(buf);

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
 * @param buf content to append
 * @param size buf's size
 * @param dirname if != NULL, store evicted files here, if any
 */
int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname)
{
    if (!pathname || !buf || (strchr(pathname,'/') != pathname))
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    p(printf("appendToFile__%s__%ld__%s: ", pathname, size, dirname));
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
    if (!pathname || (strchr(pathname,'/') != pathname))
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    p(printf("lockFile__%s: ", pathname));
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
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

/**
 * Removes O_LOCK from a file.
 * @returns 0 success, -1 error
 */
int unlockFile(const char *pathname)
{
    if (!pathname || (strchr(pathname,'/') != pathname))
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    p(printf("unlockFile__%s: ", pathname));
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
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

/**
 * Closes the given file in the storage
 */
int closeFile(const char *pathname)
{
    if (!pathname || (strchr(pathname,'/') != pathname))
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    p(printf("closeFile__%s: ", pathname));
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
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
/**
 * Removes file from the server and (if set) stores it in dirEvicted.
 * @returns 0 success, -1 error
 */
int removeFile(const char *pathname)
{
    if (!pathname || (strchr(pathname,'/') != pathname))
    {
        p(puts("Invalid argument"))
            errno = EINVAL;
        return -1;
    }
    p(printf("removeFile__%s: ", pathname));
    // SEND REQUEST
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
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
            storeFileInDir(fptr, dirEvicted);
            p(printf("File stored in %s\n", dirEvicted));
        }
        freeEvicted(fptr);
        p(printf("\tFile trashed\n"));
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
 * Assumes f->path is absolute. \n \n
 * Given a file's path "fileDir/fileName", stores it in "dirname/fileDir/fileName". \n \n
 * Creates all the needed parent directories. Overwrites file if already existing.
 * @returns 0 success, -1 error
 */
int storeFileInDir(evictedFile *f, const char *dirname)
{
    char
        *fileName = NULL,
        *lastSlash = NULL,
        *command = NULL,
        *newPath = NULL,
        firstFileNameChar;
    FILE *fptr = NULL;

    // CHECK: if it is a valid absolute path it must start with '/'
    ec_n(strchr(f->path, '/'), f->path, errno = EINVAL; goto store_cleanup);

    // We need to get a string "dirname/fileDir/"

    ec_z(newPath = calloc(strnlen(f->path, PATH_MAX) +
                              strnlen(dirname, PATH_MAX) + 1,
                          sizeof(char)),
         goto store_cleanup);
    ec_z(snprintf(newPath, PATH_MAX, "%s%s", dirname, f->path), goto store_cleanup);

    // newPath now looks like "dirname/filedir/filepath"
    ec_z(lastSlash = strrchr(newPath, '/'), goto store_cleanup);
    firstFileNameChar = *(lastSlash + 1); // backup this character
    *(lastSlash + 1) = '\0';              // truncate fileName
    // newPath now looks like "dirname/fileDir/"

    // create directory + parents
    char sh_mkdir[] = "mkdir -p "; // -p makes parents, no error if existing
    ec_z(command = calloc(
             strlen(sh_mkdir) + strnlen(newPath, PATH_MAX) + 1,
             sizeof(char)),
         goto store_cleanup);

    ec_neg(snprintf(command, strlen(sh_mkdir) + PATH_MAX, "%s%s", sh_mkdir, newPath), goto store_cleanup);
    ec_nz(system(command), goto store_cleanup);

    // Directories created,
    // now to open the file we need the entire path back
    *(newPath + strnlen(newPath, PATH_MAX)) = firstFileNameChar; // eliminate null character to get entire string again

    // WRITE FILE
    ec_z(fptr = fopen(newPath, "w+"), goto store_cleanup);
    ec_n(fwrite(f->content, sizeof(char), f->size, fptr), f->size, goto store_cleanup);
    ec_n(fclose(fptr), 0, goto store_cleanup);
    fptr = NULL;

    free(command);
    free(newPath);
    free(fileName);
    return 0;

store_cleanup:
    free(command);
    free(newPath);
    free(fileName);
    if (fptr)
        fclose(fptr);
    return -1;
}

int readFromDisk(const char *path, void **toRet, size_t *size)
{
    if (!path || !toRet || !size)
    {
        errno = EINVAL;
        return -1;
    }
    FILE *fptr = NULL;
    *toRet = NULL;
    // OPEN FILE and get SIZE
    ec_z(fptr = fopen(path, "r"), return -1);
    ec_neg1(fseek(fptr, 0L, SEEK_END), goto read_cleanup);
    *size = ftell(fptr);
    rewind(fptr); // Back to the beginning to read later

    // ALLOCATE BUF
    ec_z(*toRet = malloc(*size * 1), goto read_cleanup);

    // READ
    ec_n(fread(*toRet, 1, *size, fptr), *size, goto read_cleanup);

    ec_n(fclose(fptr), 0, goto read_cleanup);
    return 0;

read_cleanup:
    free(*toRet);
    if (fptr)
        fclose(fptr);
    return -1;
}