#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig)
{ (void)sig; g_stop = 1; }

static long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

// srv_et 같은 호스트명도 되게끔 getaddrinfo 사용
static int connect_to(const char *host, int port){
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;  //IPv4
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, portstr, &hints, &res);
    if(rc != 0) {
        fprintf(stderr, "getaddrinfo(%s:%s) failed: %s\n", host, portstr, gai_strerror(rc));
        return -1;
    }

    int fd = -1;
    for(struct addrinfo *p = res; p; p = p->ai_next){
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(fd < 0) continue;

        // SIGPIPE 방지 (서버가 끊어도 프로세스가 죽는거 방지)
#ifdef SO_NONSIGPIPE
        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_NONSIGPIPE, &one, sizeof(one));
#endif

        if(connect(fd, p->ai_addr, p->ai_addrlen) == 0){
            freeaddrinfo(res);
            return fd;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(res);
    return -1;
}

// MSG_NONSIGNAL이 리눅스에선 SIGPIPE 방지에 좋음
static ssize_t send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char*)buf;
    size_t off = 0;
    while (off < len)
    {
        ssize_t n = send(fd, p + off, len - off,
#ifdef MSG_NOSIGNAL
            MSG_NOSIGNAL
#else
            0
#endif
        );
        if(n > 0) {
            off += (size_t)n;
            continue;
        }
        if(n == 0) return 0;
        if(errno == EINTR) continue;
        return -1;
    }
    return (ssize_t)off;
}

static void usage(const char *prog) {
    fprintf(stderr,
    "Usage: %s <host> <port> [conns] [duration_sec] [msg_bytes] [rate_per_sec] [do_recv] [burst]\n"
    "   conns        : default 50"
    "   duration_sec : default 30 (0이면 무한)\n"
    "   msg_bytes    : default 64  (최소 2, 마지막은 \\n 포함)\n"
    "   rate_per_sec : default 20  (초당 전송 횟수)\n"
    "   do_recv      : default 0   (1이면 recv도 조금 돌림)\n"
    "   burst        : default 1 (1이면 라운드-로빈 버스트, 0이면 기존처럼 각 fd에 sleep포함)"
    "Example: %s srv_et 9000 30 128 50 0\n",
            prog, prog);
}

int main(int argc, char **argv) {
    if(argc < 3) { usage(argv[0]); return 1;}

    const char *host = argv[1];
    int port = atoi(argv[2]);

    int conns = (argc >= 4) ? atoi(argv[3]) : 50;
    int duration = (argc >= 5) ? atoi(argv[4]) : 30;
    int msg_bytes = (argc >= 6) ? atoi(argv[5]) : 64;
    int rate = (argc >= 7) ? atoi(argv[6]) : 20;
    int do_recv = (argc >= 8) ? atoi(argv[7]) : 0;
    int burst = (argc >= 9) ? atoi(argv[8]) : 1;

    if(conns < 1) conns = 1;
    if(msg_bytes < 2) msg_bytes = 2;
    if(rate < 1) rate = 1;

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    int *fds = calloc((size_t)conns, sizeof(int));
    if(!fds) { perror("calloc"); return 3; }

    int ok = 0;
    for(int i = 0; i < conns; i++){
        int fd = connect_to(host, port);
        if(fd < 0){
            fprintf(stderr, "connect failed at %d/%d\n",i ,conns);
            break;
        }
        fds[i] = fd;
        ok++;
    }
    if(ok == 0){
        fprintf(stderr, "no connections\n");
        free(fds);
        return 2;
    }
    
    //메세지 만들기: [pid] + payload + '\n'
    char *msg = (char*)malloc((size_t)msg_bytes);
    if(!msg) { perror("malloc"); free(fds); return 3;}

    //payload 채우기
    memset(msg, 'A', (size_t)msg_bytes);
    msg[msg_bytes - 1] = '\n';

    char head[32];
    int h1 = snprintf(head, sizeof(head), "[%d] ", getpid());
    for(int i = 0; i < h1 && i < msg_bytes - 1; i++) msg[i] = head[i];

    long long start = now_ms();
    long long end_ms = (duration == 0) ? 0 : start + (long long)duration * 1000LL;

    //rate 맞추려고 tick 단위 sleep
    long long interval_us = 1000000LL / rate;   // 초당 rate 회
    if(interval_us < 1000) interval_us = 1000;  // 너무 빡빡하면 최소 1ms

    char rbuf[4096];
    
    long long sent_cnt = 0;
    long long sent_bytes = 0;

    while(!g_stop){
        if(end_ms && now_ms() >= end_ms)
            break;

        if(burst){
            //라운드 단위: conns개 전부 한 번씩 send
            for(int i = 0; i < conns; i++){
                if(fds[i] < 0) continue;
                ssize_t n = send_all(fds[i], msg, (size_t)msg_bytes);
                if(n <= 0){
                    close(fds[i]);
                    fds[i] = -1;
                    continue;
                }
                sent_cnt++;
                sent_bytes += n;

                if(do_recv){
    #ifdef MSG_DONTWAIT
                    (void)recv(fds[i], rbuf, sizeof(rbuf), MSG_DONTWAIT);
    #endif
                }
            }
            
            usleep((useconds_t)interval_us);    // 라운드 pace
        } else {
            for(int i = 0; i < conns; i++){
                if(fds[i] < 0) continue;
                ssize_t n = send_all(fds[i], msg, (size_t)msg_bytes);
                if(n <= 0){ close(fds[i]); fds[i] = -1; continue;}
                sent_cnt++;
                sent_bytes += n;
                if(do_recv){
    #ifdef MSG_DONTWAIT
                    (void)recv(fds[i], rbuf, sizeof(rbuf), MSG_DONTWAIT);
    #endif
                }
                usleep((useconds_t)interval_us);
            }
        }

        // 2) 옵션 recv: 서버 브로드캐스트가 몰릴 때 write 막힘/버퍼 입력 줄이기
        
    }

    long long elapsed = now_ms() - start;
    if(elapsed <= 0) elapsed = 1;

    fprintf(stderr, 
        "client pid=%d done, sent=%lld msgs, %lld bytes, elapsed=%lld ms, msg_rates=%.1f msg/s, bytes/s=%.1f\n",
        getpid(), sent_cnt, sent_bytes, elapsed, (double)sent_cnt * 1000.0 / (double)elapsed, (double)sent_bytes / (double)elapsed);
    
    free(msg);
    for(int i = 0; i < conns; i++){
        if(fds[i] >= 0) close(fds[i]);
    }
    free(fds);
    return 0;
}