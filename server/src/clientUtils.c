#include <clientUtils.h>
#include <time.h>



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
    ec_neg1(system(command), goto store_cleanup);

    // Directories created,
    // now to open the file we need the entire path back
    // eliminate null character to get entire string again
    *(newPath + strnlen(newPath, PATH_MAX)) = firstFileNameChar;

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

/**
 * If 'path' contains dots '.' and doesn't exist on disk, expansion fails and NULL
 * is returned
 * @returns absolute path on success, NULL on error
 */
char *getAbsolutePath(const char *path)
{
    if (!path)
        return NULL;
    char *absPath = NULL;
    size_t len = 0;
    if (path != strchr(path, '/') && !strchr(path, '.')) // path is relative and no dots to be expanded
    {
        // realpath fails if the file doesn't actually exist...
        // so, expand manually
        char cwd[PATH_MAX];
        ec_z(getcwd(cwd, sizeof(cwd)), return NULL);
        ec_z(absPath = calloc(strnlen(path, PATH_MAX) + strnlen(cwd, PATH_MAX) + 2, sizeof(char)),
             return NULL);
        ec_neg(snprintf(absPath, PATH_MAX, "%s/%s", cwd, path), return NULL);
        return absPath;
    }
    // some dots may have to be expanded
    if (path == strchr(path, '/')) // path is absolute
    {
        len = strnlen(path, PATH_MAX) + 1;
    }
    else
        len = PATH_MAX;
    ec_z(absPath = calloc(len, sizeof(char)), return NULL);
    char *toRet = realpath(path, absPath);
    if (!toRet)
        free(absPath);
    return toRet;
}

int printString(const char *str, size_t len)
{
    for (size_t i = 0; i < len; i++)
        ec_neg(printf("%c", str[i]), return -1);

    return 0;
}
int printEvictedPath(void *arg)
{
    evictedFile *c = arg;
    ec_neg(printf(BMAG "EVCTD -- PATH: %s | CONTENT SIZE: %ld\n" REG, c->path, c->size), return -1);
    return 0;
}

/**
 * Useful exclusively for text-only files
 */
int printEvicted(void *arg)
{
    evictedFile *c = arg;
    ec_neg(printf(BMAG "EVCTD -- PATH: %s | CONTENT: " REG BMAG, c->path), return -1);
    ec_neg(printString(c->content, c->size), return -1);
    ec_neg(printf(REG "\n"), return -1);
    errno = 0;
    return 0;
}
void freeEvicted(void *arg)
{
    if (!arg)
        return;
    evictedFile *f = arg;
    free(f->content);
    free(f->path);
    f->content = NULL;
    f->path = NULL;
    free(f);
}