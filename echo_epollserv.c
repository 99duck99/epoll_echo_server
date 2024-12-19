#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define BUF_SIZE 4
#define EPOLL_SIZE 50
void error_handling(char* buf);
void setnonblockingsock(int fd);

int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t adr_sz;
    int str_len, i;
    char buf[BUF_SIZE];

    struct epoll_event *ep_events;
    struct epoll_event event;
    int epfd, event_cnt;

    if(argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(-1);
    }
    /****************서버 소켓 생성*************** */
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if(listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    /******************epoll 생성 및 크기 초기화****************************/
    epfd = epoll_create(EPOLL_SIZE);
    ep_events = malloc(sizeof(struct epoll_event)*EPOLL_SIZE);
    
    /******************server socket을 논 블로킹으로 변화******************************************************/
    setnonblockingsock(serv_sock);
    event.events = EPOLLIN;
    event.data.fd = serv_sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);

    while(1)
    {
        /***************변화(이벤트)가 감지된 소켓 검사*********************************************************/
        event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
        if(event_cnt == -1)
        {
            puts("epoll_wait() error");
            break;
        }

        puts("return epoll_wait");
        for(i = 0; i < event_cnt; i++)
        {
            if(ep_events[i].data.fd == serv_sock)
            {
                /********클라이언트 소켓 엣지트리거로 변환및 논블로킹 설정***************************************************/
                adr_sz = sizeof(clnt_adr);
                clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &adr_sz);
                setnonblockingsock(clnt_sock);
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = clnt_sock;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
                printf("connect client : %d\n", clnt_sock);
            }
            else
            {
                /***********클라이언트에서 보낸 문자열 읽기및 쓰기****************************************** */
                memset(buf, 0, sizeof(buf));
                str_len = read(ep_events[i].data.fd, buf, BUF_SIZE);
                if(str_len == 0) // close requst
                {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, ep_events[i].data.fd, NULL);
                    close(ep_events[i].data.fd);
                    printf("Closed client: %d\n", ep_events[i].data.fd);
                }
                else
                {
                    printf("%s, %d\n", buf, str_len);
                    write(ep_events[i].data.fd, buf, str_len);
                    //printf("2. %s, %d\n", buf, str_len);
                }
            }
        }
    }
    close(serv_sock);
    close(epfd);
    free(ep_events); // 프로그램 종료 전에 메모리 해제
    return 0;
}

// 소켓을 논블로킹으로 만들기
void setnonblockingsock(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag|O_NONBLOCK);
    
}

void error_handling(char* message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(-1);
}
