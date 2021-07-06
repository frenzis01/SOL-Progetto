#include <client.h>

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

char *getAbsolutePath(char *path);
int writeFilesList(queue *files, const char *dirname);
int readFilesList(queue *files, const char *dirname);
int removeFilesList(queue *files);
int lockFilesList(queue *files, _Bool lock);
int getFilesFromDir(const char *dirname, int n, queue **plist);

int main(int argc, char **argv)
{
    queue *options = NULL, *args = NULL;
    Option *op = NULL, *tmp = NULL;
    char *path = NULL;
    char *dirname = NULL;

    int t = 0;
    struct timespec interval;
    interval.tv_nsec = 0;
    interval.tv_sec = 0;

    ec_neg1(parser(argc, argv, &options), goto cleanup);
    while (!queueIsEmpty(options))
    {
        nanosleep(&interval, NULL);
        ec_z(op = queueDequeue(options), goto cleanup);
        switch (op->flag)
        {
        case 'h':
            printUsage(argv[0]);
            break;
        case 'f': // SOCKET CONN
        {
            struct timespec expire;
            expire.tv_sec = time(NULL) + 60; 
            // TODO OK? auto-expire after a 60s and 100ms if 0 to avoid flooding?
            ec_neg1(openConnection((char*)op->arg, t ? t : 100, expire), goto cleanup);
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
            ec_neg1(getFilesFromDir(wDir, x, &args), goto cleanup);
            ec_neg1(writeFilesList(args, dirname), goto cleanup);

            free(wDir);
            args = NULL;

            CLEAN_DFLAG;
            break;
        }
        case 'W': // SENDS F_LIST TO SERVER
            args = op->arg;
            GET_DFLAG;
            ec_neg1(writeFilesList(args, dirname), goto cleanup);
            CLEAN_DFLAG;
            break;
        case 'r': // READS F_LIST FROM SERVER
            GET_DFLAG;
            readFilesList(op->arg, dirname);
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
            lockFilesList(op->arg, 1);
            break;
        case 'u': // RELEASES O_LOCK ON A F_LIST
            lockFilesList(op->arg, 0);
            break;
        case 'c': // REMOVES F_LIST FROM SERVER
            removeFilesList(op->arg);
            break;
        case 'p': // ENABLES clientApi PRINTS
            pFlag = 1;
            break;
        case 'E': // SETS DIR FOR F_EVICTED BY openFile OR removeFile
            free(dirEvicted);
            dirEvicted = op->arg ? op->arg : NULL;
            break;
        default:;
        }
        free(op);
        queueDestroy(args);
        free(path);
        args = NULL;
        path = NULL;
    }

    queueDestroy(options);
    free(dirEvicted);
    free(dirname);
    return 0;

cleanup:
    free(op);
    queueDestroy(options);
    queueDestroy(args);
    free(path);
    free(dirEvicted);
    return -1;
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
    ec_z(absPath = calloc(strnlen(path, PATH_MAX) + strnlen(cwd, PATH_MAX) + 2, sizeof(char)),
         return NULL);
    // strncat(absPath, cwd, PATH_MAX);
    // strncat(absPath+strnlen(absPath,PATH_MAX), path, PATH_MAX);
    ec_neg(snprintf(absPath, PATH_MAX,"%s/%s",cwd,path), return NULL);
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
        ec_z(path = getAbsolutePath(buf), goto cleanup);
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
                ec_neg1(readFromDisk(path, (void **)&buf, &size), goto cleanup); // 'recycle' buf
                ec_api(appendToFile(path, buf, size, dirname));
                free(buf);
                buf = NULL;
            }
        }
        free(path);
        path = NULL;
    }
    queueDestroy(files);
    return 0;

cleanup:
    free(buf);
    free(path);
    return -1;
}

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
    char *buf = NULL, *path = NULL;
    int res = 0;
    size_t size = 0;
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
            ec_api(readFile(path, (void **)&buf, &size));
            evictedFile *f = evictedWrap(path, buf, size);
            if (dirname)
                ec_neg1(storeFileInDir(f, dirname), goto cleanup);
        }
        free(path);
        path = NULL;
    }
    queueDestroy(files);
    return 0;

cleanup:
    free(buf);
    free(path);
    return -1;
}

/**
 * Tries to set or remove O_LOCK from a list of file
 * @param files paths' list
 * @param lock 1 => lockFile, 0 => unlockFile
 */
int removeFilesList(queue *files)
{
    char *buf = NULL, *path = NULL;
    while (!queueIsEmpty(files))
    {
        ec_z(buf = queueDequeue(files), continue);
        ec_z(path = getAbsolutePath(buf), goto cleanup);
        free(buf);
        buf = NULL;

        // if set, removed file will be store in dirEvicted
        ec_api(lockFile(path)); // TODO ok?
        ec_api(removeFile(path));

        free(path);
        path = NULL;
    }
    queueDestroy(files);
    return 0;

cleanup:
    free(buf);
    free(path);
    return -1;
}

/**
 * Tries to set or remove O_LOCK from a list of file
 * @param files paths' list
 * @param lock 1 => lockFile, 0 => unlockFile
 */
int lockFilesList(queue *files, _Bool lock)
{
    char *buf = NULL, *path = NULL;
    while (!queueIsEmpty(files))
    {
        ec_z(buf = queueDequeue(files), continue);
        ec_z(path = getAbsolutePath(buf), goto cleanup);
        free(buf);
        buf = NULL;

        // TRY TO O_CREAT and O_LOCK THE FILE
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
    free(buf);
    free(path);
    return -1;
}

int specialDir (const char *path) {
    char *fname = strrchr(path,'/');
    if (!strncmp(fname,"/..", PATH_MAX) || !strncmp(fname,"/.", PATH_MAX)) return 1;
    return 0;
}

int specialFile (const char *name) {
    if (!strncmp(name,"..", PATH_MAX) || !strncmp(name,".", PATH_MAX)) return 1;
    return 0;
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
    _Bool all = (n > 0) ? 0 : 1;
    while ((all || n--) && (file = readdir(dir)) != NULL)
    {
        if (specialFile(file->d_name)) continue;
        ec_z(path = malloc((dirLen + strnlen(file->d_name, PATH_MAX) + 2) * sizeof(char)), goto cleanup);
        ec_neg(snprintf(path, PATH_MAX, "%s/%s", dirname, file->d_name), goto cleanup);
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