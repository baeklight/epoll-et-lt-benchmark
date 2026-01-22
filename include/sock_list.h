#ifndef SOCK_LIST_H
#define SOCK_LIST_H

#include "sock.h"

typedef struct 
{
    sock_t *head;
    sock_t *tail;
    int cnt;
} sock_list_t;

void list_init(sock_list_t *list);
void list_push_back(sock_list_t *list, sock_t *node);
void list_remove(sock_list_t *list, sock_t *node);

#endif
