#include "chat_logic.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define BUF_SIZE 4096

static void broadcast_prefixed(const char *prefix, sock_t *sender, size_t msglen, sock_list_t *list){
    // prefix 최대 32, msg 최대 INBUF_SIZE
    char out[32 + INBUF_SIZE];
    size_t plen = strlen(prefix);

    if(plen + msglen > sizeof(out)){
        msglen = sizeof(out) - plen;
    }

    memcpy(out, prefix, plen);
    memcpy(out + plen, sender->inbuf, msglen);

    for(sock_t *cur = list->head; cur; cur = cur->next){
        if(list->cnt == 1) break;
        if(cur != sender) {
            (void)write(cur->fd, out, plen + msglen);
        }
    }
}

static void process_lines(sock_t *c, sock_list_t *list){
    while(1){
        char *n1 = memchr(c->inbuf, '\n', c->inlen);
        if(!n1) break;

        size_t linelen = (size_t)(n1 - c->inbuf) + 1;

        char prefix[32];
        snprintf(prefix, sizeof(prefix), "[USER%d] ", c->id);

        broadcast_prefixed(prefix, c, linelen, list);

        size_t remain = c->inlen - linelen;
        memmove(c->inbuf, c->inbuf + linelen, remain);
        c->inlen = remain;
    }
}

int on_readable(sock_t *c, int is_et, sock_list_t *list){
    char buf[BUF_SIZE];

    while(1){
        ssize_t n = read(c->fd, buf, sizeof(buf));      // read 반환값을 고려하여 ssize_t 사용

        if(n > 0) {
            if(c->inlen + (size_t)n > INBUF_SIZE){
                // 버퍼 초과: "드랍하고 커넥트 유지"
                c->inlen = 0;
                return 0;
            }

            memcpy(c->inbuf + c->inlen, buf, (size_t)n);
            c->inlen += (size_t)n;

            process_lines(c, list);

            if(!is_et) break; // LT면 한번만 처리
        } else if (n == 0){
            return -1; // peer closed
        } else {
            if(errno == EINTR) continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;

            // 그 외 에러는 종료
            return -1;
        }
    }
    return 0;
}