#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>

#define PORT 9000

int main(void){     //추후에 argc랑 argv[] 사용해서 코드 써보기 
    int sock = 0;
    int valread;
    struct sockaddr_in address;

    fd_set reads, temps;
    int fd_max;
    struct timeval timeout;

    int fd;

    sock = socket(PF_INET, SOCK_STREAM, 0);     //AF_INET대신 PF_INET
    if(sock < 0)
    {
        printf("socket creation error");
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    
    struct hostent *host = gethostbyname("srv_et");
    if(host == NULL){
        perror("gethostbyname");
        exit(1);
    }
    memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);

    char ipbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, ipbuf, sizeof(ipbuf));
    fprintf(stderr, "CONNECTING -> %s:%d\n", ipbuf, ntohs(address.sin_port));

    if(connect(sock, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("connect error");
        return -1;
    }

    FD_ZERO(&reads);
    FD_SET(sock, &reads);
    FD_SET(0, &reads);
    fd_max = sock;
    int running = 1;

    while(running)      //while문 조건 true에서 running으로 대체 read()함수 반환값에 따른 분기를 위함
    {
        char buffer[1024] = {0};
        char talk[1024] = {0};

        temps = reads;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        if(select(fd_max + 1, &temps, NULL, NULL, &timeout) < 0)
        {
            perror("select error");
            exit(1);
        }

        for(fd = 0; fd < fd_max + 1; fd++)      //select()후 fd_set돌면서 발생한 이벤트 식별
        {
            if(FD_ISSET(fd, &temps))
            {
                if(fd == sock)      //socket에 이벤트 발생 시
                {
                    valread = read(sock, buffer, sizeof(buffer));
                    if(valread == 0)      //클라이언트이기 때문에 연결종료시 리소스해제하고 프로그램 종료
                    {
                        printf("server closed connection\n");
                        close(sock);
                        running = 0;
                        break;
                    }
                    else if(valread < 0)
                    {   
                        if(errno == EINTR){     //signal에 의해 syscall이 끊긴 경우
                            continue;
                        }
                        else
                        {
                            perror("read error");
                            close(sock);
                            break;
                        }
                       
                    }
                    else
                    {
                        write(STDOUT_FILENO, buffer, valread);
                    }
                }
                else        //키보드 이벤트 발생 시
                {
                    if(fgets(talk, sizeof(talk), stdin) != NULL){
                        write(sock, talk, strlen(talk));
                    } else {
                        while(1){
                            sleep(1);
                        }
                    }
                }
            }
        }

    }
    return 0;
}