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

#define FORMAT_STR "%c%04d%02hu%010ld%02hu%s%s%s"

char skname[UNIX_PATH_MAX] = ""; // active connection
int skfd = 0;
_Bool pFlag = 1;

int readResponseCode(char res);

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
    interval.tv_sec = 0;
    interval.tv_nsec = msec * 1000000;

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
    if ((flags < 0 || flags > 3) || !pathname)
    {
        errno = EINVAL;
        return -1;
    }
    unsigned short pathLen = strnlen(pathname, PATH_MAX);
    printf("%hd %s\n",pathLen,pathname);
    size_t reqLen = REQ_LEN_SIZE + pathLen*sizeof(char) + 1;
    const char cflags = flags;
    char *req;
    ec_z(req = genRequest(reqLen, OPEN_FILE, cflags, 0, pathLen, 0, 0, pathname, "", ""), return -1);
    // if(snprintf(req, reqLen, "%c%c%04d%02hu%02hu%010ld%s%s%s" ,OPEN_FILE,cflags,0,pathLen,0,(size_t*)0,pathname,"","") < 0) {
    //     return -1;
    // }
    char res = 0;
    ec_neg1(writen(skfd, req, reqLen), return -1);
    free(req);
    ec_neg1(readn(skfd, &res, sizeof(char)), return -1);
    p(readResponseCode(res));
    return 0;
}

int readNFiles(int N, const char *dirname);
int writeFile(const char *pathname, const char *dirname);
int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname);
int lockFile(const char *pathname);
int unlockFile(const char *pathname);
int closeFile(const char *pathname);
int removeFile(const char *pathname);

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
    index += appendLen;

    return buf;
}

/**
 * Reads response code
 */
int readResponseCode(char res)
{
    switch (res)
    {
    case FILE_NOT_FOUND:
        puts("FILE_NOT_FOUND");
        break;
    case NO_ACCESS:
        puts("NO_ACCESS");
        break;
    case TOO_BIG:
        puts("TOO_BIG");
        break;
    case ALREADY_EXISTS:
        puts("ALREADY_EXISTS");
        break;
    case SUCCESS:
        puts("SUCCESS");
        break;
    default:
        puts("Something bad happened");
        break;
    }
    return 0;
}