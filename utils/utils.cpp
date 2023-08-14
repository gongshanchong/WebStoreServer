#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "../log/Log.h"

void errif(bool condition, const char *errmsg){
    if(condition){
        perror(errmsg);
        LOG_ERROR(errmsg);
        exit(EXIT_FAILURE);
    }
}

void setNonBlocking(int argFd) {
    errif(fcntl(argFd, F_SETFL, fcntl(argFd, F_GETFL) | O_NONBLOCK) == -1, "Socket set non-blocking failed");
}

bool IsNonBlocking(int argFd) { 
    return (fcntl(argFd, F_GETFL) & O_NONBLOCK) != 0; 
}



