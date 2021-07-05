#include <clientUtils.h>

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