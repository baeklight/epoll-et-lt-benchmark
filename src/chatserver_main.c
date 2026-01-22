#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>

#include "sock.h"
#include "sock_list.h"
#include "netutil.h"
#include "chat_logic.h"

#define EPOLL_SIZE 501

static void close_client(int epfd, sock_list_t *list, sock_t *c){
    epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);
    close(c->fd);
    list_remove(list, c);
    free(c);
}

int main(int argc, char *argv[]){
    signal(SIGPIPE, SIG_IGN);
    
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t adr_sz;

    int is_et = 0;  // ET : is_et = 1, LT : is_et = 0

    struct epoll_event *ep_events;
    struct epoll_event event;
    int epfd, event_cnt;

    int clnt_cnt = 1;

    sock_list_t c_list;
    list_init(&c_list);

    if(argc != 2){
        fprintf(stderr, "Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1) die("socket");

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    int opt = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if(bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1){
        fprintf(stderr, "bind failed: errno = %d (%s)\n", errno, strerror(errno));
        exit(1);
    }
    if(listen(serv_sock, 1024) == -1)
        die("listen");
    
    epfd = epoll_create(EPOLL_SIZE);
    if(epfd == -1) die("epoll create");

    ep_events = malloc(sizeof(struct epoll_event) * EPOLL_SIZE);
    if(!ep_events) die("malloc ep_events");

    set_nonblocking(serv_sock);

    sock_t *L = malloc(sizeof(*L));
    memset(L, 0, sizeof(*L));
    L->fd = serv_sock;
    L->is_listener = 1;

    event.events = EPOLLIN;
    event.data.ptr = L;
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event) == -1) die("epoll_ctl ADD listener");

    while(1){
        event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
        if(event_cnt == -1){
            if(errno == EINTR) continue;
            die("epoll_wait");
        }

        for(int i = 0; i < event_cnt; i++){
            sock_t *S = (sock_t *)ep_events[i].data.ptr;

            if(!S->is_listener){
                if(ep_events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)){
                    close_client(epfd, &c_list, S);
                    continue;
                }
            }

            if(S->is_listener){
                adr_sz = sizeof(clnt_adr);
                clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &adr_sz);
                if(clnt_sock == -1){
                    continue;
                }

                set_nonblocking(clnt_sock);

                sock_t *C = malloc(sizeof(*C));
                if(!C) die("malloc client");
                memset(C, 0, sizeof(*C));

                C->is_listener = 0;
                C->fd = clnt_sock;
                C->id = clnt_cnt++;
                C->next = NULL;

                event.events = EPOLLIN | (is_et ? EPOLLET : 0);
                event.data.ptr = C;
                if(epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event) == -1)
                    die("epoll_ctl ADD client");
                
                list_push_back(&c_list, C);
                printf("conneccted client %d\n", clnt_sock);
            } else {
                if(on_readable(S, is_et, &c_list) == -1){
                    close_client(epfd, &c_list, S);
                }
            }
        }
    }
    close(serv_sock);
    close(epfd);
    free(ep_events);
    free(L);
    return 0;
}