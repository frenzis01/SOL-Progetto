#include <client.h>

#include <utils.h>
#include <parser.h>
#include <clientApi.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#define FILESYS_err (errno == EACCES || errno == ENOENT || errno == EADDRINUSE || errno == EFBIG)
#define ec_api(s)                          \
    do                                     \
    {                                      \
        if ((s) == (-1) && !(FILESYS_err)) \
        {                                  \
            perror(#s);                    \
            goto cleanup;                  \
        }                                  \
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

int main(int argc, char **argv)
{
    queue *options = NULL, *args = NULL;
    Option *op = NULL, *tmp = NULL;
    char *path = NULL;
    char *dirname = NULL;
    int t = 0;
    ec_neg1(parser(argc, argv, &options), goto cleanup);
    while (!queueIsEmpty(options))
    {
        ec_z(op = queueDequeue(options), goto cleanup);
        switch (op->flag)
        {
        case 'h':
            printUsage(argv[0]);
            break;
        case 'f': // SOCKET CONN
            struct timespec expire;
            expire.tv_sec = time(NULL) + 60; // TODO auto-expire after a 60s?
            ec_neg1(openConnection(op->arg, t, expire), goto cleanup);
            break;
        case 'w': // READS N FILES FROM DIR and SENDS THEM
            char *wDir = queueDequeue(op->arg);
            int *n = queueDequeue(op->arg);
            queueDestroy(op->arg);
            GET_DFLAG;

            ec_neg1(getFilesFromDir(wDir, *n, &args), goto cleanup);
            ec_neg1(writeFilesList(args, dirname), goto cleanup);

            CLEAN_DFLAG;
            break;
        case 'W': // SENDS LIST OF FILES TO THE SERVER
            args = op->arg;
            GET_DFLAG;

            ec_neg1(writeFilesList(args, dirname), goto cleanup);

            CLEAN_DFLAG;
            break;
        case 'r':

        default:;
        }
        queueDestroy(args);
        free(path);
        args = NULL;
        path = NULL;
    }

cleanup:
    queueDestroy(op);
    queueDestroy(args);
    free(path);
}

/**
 * If path is a relative path, frees path and returns corresponding absolute path
 */
char *getAbsolutePath(char *path)
{
    if (!path)
        return NULL;
    if (path == strchr(path, '/')) // path is absolute
        return path;
    char cwd[PATH_MAX];
    ec_z(getcwd(cwd, sizeof(cwd)), free(path); return NULL);
    char *absPath = NULL;
    ec_z(absPath = malloc((strnlen(path, PATH_MAX) + strnlen(cwd, PATH_MAX)) * sizeof(char)),
         return -1);
    strncat(absPath, cwd, PATH_MAX);
    strncat(absPath, path, PATH_MAX);
    return absPath;
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
        ec_z(buf = queueDequeue(files), continue);
        ec_z(getAbsolutePath(buf), goto cleanup);
        free(buf);
        buf = NULL;

        // TRY TO O_CREAT and O_LOCK THE FILE
        ec_api(res = openFile(path, _O_CREAT | _O_LOCK));
        if (res == SUCCESS)
        {
            ec_api(writeFile(path, dirname));
        }
        else
        {
            ec_api(res = openFile(path, 0));
            if (res == SUCCESS)
            {
                size_t size;
                ec_neg1(readFromDisk(path, &buf, &size), goto cleanup); // 'recycle' buf
                ec_api(appendToFile(path, buf, size, dirname));
                free(buf);
                buf = NULL;
            }
        }
        free(path);
    }
    queueDestroy(files);
    return 0;

cleanup:
    free(buf);
    free(path);
    return -1;
}

/**
 * Reads a list of files from disk and sends it to server.
 * Tries to openFile with O_CREAT and O_LOCK first.
 * If it fails, it tries to openFile with no flags and then appendToFile.
 * @param files paths' list
 * @param dirname if != NULL stores evicted files (if any) here 
 */
int readFilesList(queue *files, const char *dirname)
{
    char *buf = NULL, *path = NULL;
    int res = 0;
    while (!queueIsEmpty(files))
    {
        // if the option was "-d file1,,file2" strtok should have skipped both ',';
        // however, we can safely skip if, for some reason,
        // the queue contains a NULL string
        ec_z(buf = queueDequeue(files), continue);
        ec_z(getAbsolutePath(buf), goto cleanup);
        free(buf);
        buf = NULL;

        // TRY TO O_CREAT and O_LOCK THE FILE
        ec_api(res = openFile(path, _O_CREAT | _O_LOCK));
        if (res == SUCCESS)
        {
            ec_api(writeFile(path, dirname));
        }
        else
        {
            ec_api(res = openFile(path, 0));
            if (res == SUCCESS)
            {
                size_t size;
                ec_neg1(readFromDisk(path, &buf, &size), goto cleanup); // 'recycle' buf
                ec_api(appendToFile(path, buf, size, dirname));
                free(buf);
                buf = NULL;
            }
        }
        free(path);
    }
    queueDestroy(files);
    return 0;

cleanup:
    free(buf);
    free(path);
    return -1;
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
    size_t dirLen = strnlen(dirname, PATH_MAX);
    while (n-- && (file = readdir(dir)) != NULL)
    {
        ec_z(path = malloc((dirLen + strnlen(file->d_name, PATH_MAX) + 2) * sizeof(char)), goto cleanup);
        ec_neg(snprintf(path, PATH_MAX, "%s/%s", dirname, file->d_name), path);
        ec_neg1(queueEnqueue(*plist, path), goto cleanup);
    }
    ec_neg1(closedir(dir), goto cleanup);

    return 0;

cleanup:
    free(path);
    queueDestroy(*plist);
    if (dir)
        closedir(dir);
    return -1;
}