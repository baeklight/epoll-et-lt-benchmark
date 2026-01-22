#include "netutil.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

void set_nonblocking(int fd){
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

void die(const char *msg){
    perror(msg);
    exit(1);
}