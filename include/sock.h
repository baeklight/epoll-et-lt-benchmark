#ifndef SOCK_H
#define SOCK_H

#include <stdio.h>

#define INBUF_SIZE 8192

typedef struct sock {
    int fd;
    int is_listener;    // 1 : listener, 0 : client
    int id;

    size_t inlen;
    char inbuf[INBUF_SIZE];

    struct sock *next;
}sock_t;

#endif