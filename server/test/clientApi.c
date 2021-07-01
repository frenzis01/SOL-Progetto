#include <clientApi.h>
#include <protocol.h>


// Simple test used during clientApi development

#define SOCKNAME "sock"

int main () {
    struct timespec abstime;
    abstime.tv_sec = time(NULL) + 200;
    openConnection(SOCKNAME, 100, abstime);
    openFile("fileTest",2);
    closeConnection(SOCKNAME);
    return 0;
}