#include <clientApi.h>
#include <protocol.h>


// Simple test used during clientApi development

#define SOCKNAME "sock"

int main () {
    struct timespec abstime;
    abstime.tv_sec = time(NULL) + 200;
    openConnection(SOCKNAME, 1000, abstime);
    openFile("fileTest",3);
    // char buf[] = "Content test";
    // appendToFile("fileTest",buf,strlen(buf) + 1, NULL);
    // void *tmp;
    // size_t size;
    // readFile("fileTest",&tmp,&size);
    // closeConnection(SOCKNAME);
    return 0;
}