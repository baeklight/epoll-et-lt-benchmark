#ifndef CHAT_LOGIC_H
#define CHAT_LOGIC_H

#include "sock.h"
#include "sock_list.h"

// ET/LT 공통: is_et = 1이면 ET, 0이면 LT
int on_readable(sock_t *c, int is_et, sock_list_t *list);

#endif