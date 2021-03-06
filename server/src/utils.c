#include <utils.h>

#include <stdarg.h>

/** Read "n" bytes from a descriptor 
 * @returns bytes read (n - nleft), -1 on error
*/

#pragma GCC diagnostic ignored "-Wpointer-arith"
ssize_t 
readn(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nread;

    nleft = n;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; /* error, return -1 */
            else
                break; /* error, return amount read so far */
        }
        else if (nread == 0)
        {
            errno = ENOTCONN;
            break; /* EOF */
        }
        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); /* return >= 0  (bytes read) */
}

ssize_t /* Write "n" bytes to a descriptor */
writen(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;

    nleft = n;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; /* error, return -1 */
            else
                break; /* error, return amount written so far */
        }
        else if (nwritten == 0)
            break;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n - nleft); /* return >= 0 */
}
#pragma GCC diagnostic pop

/**
 * Finds a char* value after a "[string]="
 * @returns NULL on error, desired string on success
 */
char *conf_string(FILE *file, char const *desired_name) { 
    char name[128],
     val[128],
    *toRet = calloc(128, sizeof(char));

    while (fscanf(file, "%127[^=]=%127[^\n]%*c", name, val) == 2) {
        if (0 == strcmp(name, desired_name)) {
            // return strdup(val);
            strncpy(toRet,val,128);
            return toRet;
        }
    }
    return NULL;
}


/**
 * Finds a size_t value after a "[string]="
 * @returns -1 on error, desired string on success
 */
int conf_sizet(FILE *file, char const *desired_name, size_t *ret) {
    char *temp = conf_string(file, desired_name);
    if (!temp) return -1;

    char *stop;
    *ret = strtol(temp, &stop, 10);
    int ret_val = stop == temp;
    free(temp);
    return ret_val;
}

/**
 * Find maximum between two or more integer variables
 * @param args Total number of integers
 * @param ... List of integer variables to find maximum
 * @return Maximum among all integers passed
 */
int max(int args, ...){
    int i, max, cur;
    va_list valist;
    va_start(valist, args);

    max = INT_MIN;

    for(i=0; i<args; i++){
        cur = va_arg(valist, int); // Get next elements in the list
        if(max < cur)
            max = cur;
    }

    va_end(valist); // Clean memory assigned by valist

    return max;
}

/**
 * Checks if 's' is an integer and stores the corresponding value in 'n'
 * @param n where the result will be stored
 * @returns 1 success, 0 's' not an integer
 */
int isInteger(const char* s, int* n){
    char *e = NULL;
    errno = 0;
    long val = strtol(s, &e, 10);

    if (errno == ERANGE || val > INT_MAX || val < INT_MIN){
        // overflow/underflow
        return 0;
    }

    if (errno==0 && e != NULL && e != s){
        *n = (int) val;
        // ?? un numero valido
        return 1;
    }

    // non ?? un numero
    return 0;
}

/**
 * Checks if 's' is a long and stores the corresponding value in 'n'
 * @param n where the result will be stored
 * @returns 1 success, 0 's' not an integer
 */
int isLong(const char* s, long* n){
    char *e = NULL;
    errno = 0;
    long val = strtol(s, &e, 10);

    if (val == LONG_MAX || val == LONG_MIN ){
        // overflow/underflow
        return 0;
    }

    if (e != NULL && e != s){
        *n = val;
        // ?? un numero valido
        return 1;
    }

    // non ?? un numero
    return 0;
}

/**
 * Reads an entire file from disk and stores its content in *toRet and 
 * its size in *size.
 * @returns 0 success, -1 error
 */
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