#define _POSIX_C_SOURCE 200809L

#include <utils.h>
#include <parser.h>
#include <clientApi.h>
#include <queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#pragma region
#define FILESYS_err (errno == EACCES || errno == ENOENT || errno == EADDRINUSE || errno == EFBIG || errno == EINVAL)
#define ec_api(s)                                                                         \
    do                                                                                    \
    {                                                                                     \
        if (nanosleep(&interval, NULL) == (-1) || ((s) == (-1) && !(FILESYS_err)))        \
        {                                                                                 \
            if (errno && errno != ECONNRESET && errno != ENOTCONN && errno != EPIPE) \
                perror(BRED #s REG);                                                      \
            goto cleanup;                                                                 \
        }                                                                                 \
    } while (0);

#define ec_neg1_cli(s, c)                                               \
    do                                                                  \
    {                                                                   \
        if ((s) == (-1))                                                \
        {                                                               \
            if (errno == ETIME)                                         \
                perror("Connection to the server failed");                            \
            else if (errno && errno != ENOTCONN && errno != ECONNRESET) \
                perror(#s);                                             \
            c;                                                          \
        }                                                               \
    } while (0);

#define GET_DFLAG                                                               \
    do                                                                          \
    {                                                                           \
        if (options->head && (((Option *)(options->head->data))->flag == 'd' || \
                              ((Option *)(options->head->data))->flag == 'D'))  \
        {                                                                       \
            tmp = queueDequeue(options);                                        \
            dirname = (tmp)->arg;                                               \
            free(tmp);                                                          \
            tmp = NULL;                                                         \
        }                                                                       \
    } while (0);

#define CLEAN_DFLAG     \
    do                  \
    {                   \
        free(dirname);  \
        dirname = NULL; \
    } while (0);
#pragma endregion

int writeFilesList(queue *files, const char *dirname);
int readFilesList(queue *files, const char *dirname);
int removeFilesList(queue *files);
int lockFilesList(queue *files, _Bool lock);
int getFilesFromDir(const char *dirname, int n, queue **plist);
int hPresent(queue *oplist);

struct timespec interval;

int main(int argc, char **argv)
{

    // Ignore sigpipe to exit cleanly (write return value is always checked)
    struct sigaction saa;
    memset(&saa, 0, sizeof(saa));
    saa.sa_handler = SIG_IGN;
    ec_neg1(sigaction(SIGPIPE, &saa, NULL), exit(EXIT_FAILURE));

    // These will be useful later
    queue *options = NULL, *args = NULL;
    Option *op = NULL, *tmp = NULL;
    char *path = NULL,
         *dirname = NULL,
         sockname[PATH_MAX];

    int t = 0;

    interval.tv_nsec = 0;
    interval.tv_sec = 0;

    // Parse command line options to get an options queue
    ec_neg1(parser(argc, argv, &options), goto cleanup);
    if (hPresent(options)) // we have to exit without sending any request
    {
        printUsage(argv[0]);
        freeOptionsList(&options);
        return 0;
    }
    while (!queueIsEmpty(options)) // one option at a time
    {
        ec_z(op = queueDequeue(options), goto cleanup);
        switch (op->flag)
        {
        case 'f': // SOCKET CONN
        {
            struct timespec expire;
            expire.tv_sec = time(NULL) + 10;
            // auto-expire after a 10s and retry every 100ms if 0 to avoid flooding
            strncpy(sockname, op->arg, PATH_MAX);
            ec_neg1_cli(openConnection(sockname, t ? t : 100, expire), free(op->arg); goto cleanup);
            free(op->arg);
            break;
        }
        case 'w': // READS N FILES FROM DIR and SENDS THEM
        {
            char *wDir = queueDequeue(op->arg);
            int *n = queueDequeue(op->arg);
            queueDestroy(op->arg);
            GET_DFLAG;

            int x = n ? *n : 0;
            free(n);
            ec_neg1_cli(getFilesFromDir(wDir, x, &args), free(wDir); goto cleanup);
            ec_neg1_cli(writeFilesList(args, dirname), free(wDir); goto cleanup);

            free(wDir);
            args = NULL;

            CLEAN_DFLAG;
            break;
        }
        case 'W': // SENDS F_LIST TO SERVER
            GET_DFLAG;
            ec_neg1_cli(writeFilesList(op->arg, dirname), goto cleanup);
            CLEAN_DFLAG;
            break;
        case 'r': // READS F_LIST FROM SERVER
            GET_DFLAG;
            ec_neg1_cli(readFilesList(op->arg, dirname), goto cleanup);
            CLEAN_DFLAG;
            break;
        case 'R': // READS N FILES FROM SERVER
        {
            int x = op->arg ? *(int *)op->arg : 0;
            free(op->arg);
            GET_DFLAG;
            ec_api(readNFiles(x, dirname));
            GET_DFLAG;
            break;
        }
        case 't': // TIME TO ELAPSE BETWEEN REQUESTS
            t = op->arg ? *(int *)op->arg : 0;
            interval.tv_sec = t / 1000;
            interval.tv_nsec = (t % 1000) * 1000000;
            free(op->arg);
            break;
        case 'l': // ACQUIRES O_LOCK ON A F_LIST
            ec_neg1_cli(lockFilesList(op->arg, 1), goto cleanup);
            break;
        case 'u': // RELEASES O_LOCK ON A F_LIST
            ec_neg1_cli(lockFilesList(op->arg, 0), goto cleanup);
            break;
        case 'c': // REMOVES F_LIST FROM SERVER
            ec_neg1_cli(removeFilesList(op->arg), goto cleanup);
            break;
        case 'p': // ENABLES clientApi PRINTS
            pFlag = 1;
            break;
        case 'E': // SETS DIR FOR F_EVICTED BY openFile OR removeFile
            free(dirEvicted);
            dirEvicted = op->arg ? op->arg : NULL;
            break;
        default:; // Should never catch this
        }
        free(op);
        free(path);
        path = NULL;
        op = NULL;
    }

cleanup:
    closeConnection(sockname);
    free(op);
    freeOptionsList(&options);
    free(path);
    free(dirEvicted);
    free(dirname);
    return 0;
}

/**
 * Reads a list of files from disk and sends it to server.
 * Tries to openFile with O_CREAT and O_LOCK first.
 * If it fails, it tries to openFile with no flags and then appendToFile.
 * @param files paths' list
 * @param dirname if != NULL stores evicted files (if any) here 
 */
int writeFilesList(queue *files, const char *dirname)
{
    char *buf = NULL, *path = NULL;
    int res = 0;
    while (!queueIsEmpty(files))
    {
        // if the option was "-d file1,,file2" strtok should have skipped both ',';
        // however, we can safely skip if, for some reason,
        // the queue contains a NULL string

        ec_z(path = queueDequeue(files), continue);

        // TRY TO O_CREAT and O_LOCK THE FILE
        ec_api(res = openFile(path, _O_CREAT | _O_LOCK));
        if (res == SUCCESS)
        {
            ec_api(writeFile(path, dirname));
            ec_api(closeFile(path));
        }
        else if (errno == EADDRINUSE) // If it already exists then append
        {
            ec_api(res = openFile(path, 0));
            if (res == SUCCESS)
            {
                size_t size;
                ec_neg1(readFromDisk(path, (void **)&buf, &size), goto cleanup);
                ec_api(appendToFile(path, buf, size, dirname));
                free(buf);
                buf = NULL;
                ec_api(closeFile(path));
            }
        }
        free(path);
        path = NULL;
    }
    queueDestroy(files);
    return 0;

cleanup:
    queueDestroy(files);
    free(buf);
    free(path);
    return -1;
}

/**
 * Utility needed in readFilesList
 */
evictedFile *evictedWrap(char *path, char *content, size_t size)
{
    evictedFile *toRet;
    ec_z(toRet = malloc(sizeof(evictedFile)), return NULL);
    toRet->path = path;
    toRet->content = content;
    toRet->size = size;
    return toRet;
}

/**
 * Reads a list of files from the server and (if!=NULL) stores it in dirname.
 * @param files paths' list
 */
int readFilesList(queue *files, const char *dirname)
{
    void *buf = NULL;
    char *path = NULL;
    int res = 0;
    size_t size = 0;
    evictedFile *f = NULL;
    while (!queueIsEmpty(files))
    {
        ec_z(buf = queueDequeue(files), continue);
        ec_z(path = getAbsolutePath(buf), goto cleanup);
        free(buf);
        buf = NULL;

        // TRY TO O_CREAT and O_LOCK THE FILE
        ec_api(res = openFile(path, 0));
        if (res == SUCCESS)
        {
            // The files will be saved on disk by readFile
            errno = 0;
            ec_api(res = readFile(path, &buf, &size));
            if (dirname && res == SUCCESS)
            {
                ec_z(f = evictedWrap(path, buf, size), goto cleanup);
                ec_neg1(storeFileInDir(f, dirname), goto cleanup);
                free(f);
                f = NULL;
            }
            ec_api(closeFile(path));
        }
        free(path);
        free(buf);
        path = NULL;
        buf = NULL;
    }
    queueDestroy(files);
    return 0;

cleanup:
    freeEvicted(f);
    queueDestroy(files);
    return -1;
}

/**
 * Tries to set or remove O_LOCK from a list of file
 * @param files paths' list
 */
int removeFilesList(queue *files)
{
    char *path = NULL;
    while (!queueIsEmpty(files))
    {
        ec_z(path = queueDequeue(files), continue);

        // if set, removed file will be store in dirEvicted
        ec_api(lockFile(path));
        ec_api(removeFile(path));

        free(path);
        path = NULL;
    }
    queueDestroy(files);
    return 0;

cleanup:
    free(path);
    queueDestroy(files);
    return -1;
}

/**
 * Tries to set or remove O_LOCK from a list of file
 * @param files paths' list
 * @param lock 1 => lockFile, 0 => unlockFile
 */
int lockFilesList(queue *files, _Bool lock)
{
    char *path = NULL;
    while (!queueIsEmpty(files))
    {
        ec_z(path = queueDequeue(files), continue);

        if (lock)
        {
            ec_api(lockFile(path));
        }
        else
        {
            ec_api(unlockFile(path));
        }

        free(path);
        path = NULL;
    }
    queueDestroy(files);
    return 0;

cleanup:
    free(path);
    queueDestroy(files);
    return -1;
}

/**
 * @returns 1 if 'name' is ".." or "."
 */
int specialDir(const char *name)
{
    if (!strncmp(name, "..", PATH_MAX) || !strncmp(name, ".", PATH_MAX))
        return 1;
    return 0;
}
/**
 * @returns result of the concatenation of p1 and p2
 */
char *catPaths(const char *p1, const char *p2)
{
    char *path;
    ec_z(path = malloc((strnlen(p1, PATH_MAX) + strnlen(p2, PATH_MAX) + 2) * sizeof(char)), return NULL);
    ec_neg(snprintf(path, PATH_MAX, "%s/%s", p1, p2), free(path); return NULL);
    return path;
}

/**
 * Stores 'n' file paths from 'dirname' in '*plist'
 * @returns 0 success, -1 error
 */
int getFilesFromDir(const char *dirname, int n, queue **plist)
{
    DIR *dir = NULL;
    char *path = NULL;
    ec_z(*plist = queueCreate(free, NULL), return -1);
    ec_z(dir = opendir(dirname), goto cleanup;);
    struct dirent *file;
    _Bool all = (n > 0) ? 0 : 1;

    queue *subDir = NULL;
    char *curr = NULL;
    while ((all || n) && (file = readdir(dir)) != NULL)
    {
        struct stat s;
        ec_z(path = catPaths(dirname, file->d_name), goto cleanup);
        ec_neg1(stat(path, &s), goto cleanup);

        // We ignore files that aren't regular
        if ((!S_ISDIR(s.st_mode) && !S_ISREG(s.st_mode)) || specialDir(file->d_name))
        {
            free(path);
            continue;
        }
        if (S_ISDIR(s.st_mode)) // Explore dir recursively
        {
            ec_neg1(getFilesFromDir(path, n, &subDir), goto cleanup);
            free(path);
            curr = queueDequeue(subDir);
            while (curr)
            {
                ec_neg1(queueEnqueue(*plist, curr), goto cleanup);
                curr = queueDequeue(subDir);
                n--;
            }
            queueDestroy(subDir);
            subDir = NULL;
            curr = NULL;
        }
        else // is a regular file
        {
            ec_neg1(queueEnqueue(*plist, path), goto cleanup);
            n--;
        }
    }
    ec_neg1(closedir(dir), goto cleanup);

    return 0;

cleanup:
    queueDestroy(subDir);
    free(curr);
    free(path);
    queueDestroy(*plist);
    *plist = NULL;
    if (dir)
        closedir(dir);
    return -1;
}

/**
 * @returns 1 if 'h' is present in the given options queue, 0 otherwise
 */
int hPresent(queue *oplist)
{
    Option h;
    h.flag = 'h';

    if (queueFind(oplist, &h, cmpFlagOption))
        return 1;
    return 0;
}
