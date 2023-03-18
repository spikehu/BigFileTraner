#include <sys/epoll.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
//epoll 实现回声服务器

#define BACKLOG 5
#define MAXEVENT 60

//注册和绑定端口
//返回一个accept的文件描述符

struct sockaddr_in sockaddr_serv;
struct sockaddr_in sockaddr_cl;


//创建监听fd
int create_bind(char* arg);

//设置fd非阻塞
int set_fd_noblock(int fd);


int efd; // epoll 句柄
struct epoll_event event;
struct epoll_event* events;

int main(int argc ,char* argv[])
{
    if(argc!=2)
    {
        printf("Using : ./EpollPrac [PORT]\n");
        return -1;
    }
   

    int accpet_sock = create_bind(argv[1]);

    //设置为非阻塞
    if(set_fd_noblock(accpet_sock) == -1){return -1;}


    //创建句柄
    efd = epoll_create(1);
    if(efd == -1){printf("epoll_create failed\n");return -1;}


    //初始化监听端口的事件
    event.data.fd = accpet_sock;
    event.events  = EPOLLIN;
    //注册
    if(epoll_ctl(efd,EPOLL_CTL_ADD,accpet_sock,&event) == -1){printf("epoll_ctl failed\n");return -1;}
    events = (struct epoll_event*)malloc(MAXEVENT*sizeof(struct epoll_event));
    memset(events,0,sizeof(events));

    while(true)
    {
        //开始阻塞等待
        int num = epoll_wait(efd,events,MAXEVENT,-1);
        if(num == -1){printf("epool_wait failed\n");return -1;}

        //有事件发生
        for(int i = 0;i < num ; i++)
        {
            if(events[i].events & EPOLLIN) 
            {
                if(events[i].data.fd == accpet_sock) // 有客户端连接
                {
                    int clnt_fd = accept(accpet_sock,NULL,0);

                    set_fd_noblock(clnt_fd); // 设置为非阻塞

                    printf("客户端%ld接入\n",clnt_fd);
                    if(clnt_fd == -1 ){printf("accept failed\n");}
                    //进行注册
                    event.data.fd = clnt_fd;
                    event.events  = EPOLLIN | EPOLLOUT;
                    if(epoll_ctl(efd ,EPOLL_CTL_ADD, clnt_fd, &event)==-1){printf("register failed\n");return -1;}
                }else
                {
                    while(1)
                    {
                         //客户端发来消息
                        //接收并且发回去
                        char buf[101];
                        memset(buf,0,sizeof(buf));
                        int clnt_fd = events[i].data.fd;
                        int n = recv(clnt_fd,buf,100,0);
                        if(n==-1)
                        {
                                if(errno == EAGAIN || errno == EWOULDBLOCK)
                                {
                                    //读取完毕
                                    break;
                                }
                                close(clnt_fd);
                                break;
                        }
                        else if(n==0)
                        {
                            printf("%ld 断开连接\n",clnt_fd);
                            close(clnt_fd);
                            break;
                        }else
                        {
                                send(clnt_fd,buf,n,0);
                        }
                       
                    }
                   
                }
               
            }
        }
    }

    return 0;
}


int create_bind(char* arg)
{
    int port = atoi(arg);

    int sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock == -1)
    {
        printf("ctreate_bind:socket() failed\n");
        exit(-1);
    }

    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR ,NULL,0);
    //初始化地址
    sockaddr_serv.sin_family = AF_INET;
    sockaddr_serv.sin_port = htons(port);
    sockaddr_serv.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t socklen= sizeof(sockaddr_serv);
    
    //绑定
    if(bind(sock,(struct sockaddr*)&sockaddr_serv, socklen)==-1)
    {
        printf("ctreate_bind:bind error() failed\n");
        exit(-1);
    }
    listen(sock,BACKLOG);
    return sock;
}

//设置fd非阻塞
int set_fd_noblock(int fd)
{
    int fd_mod = fcntl(fd , F_GETFL,0);
    if(fd_mod == -1){printf("set_fd_noblock: fcntl() failed\n");return -1;}

    //设置为非阻塞
    fd_mod |= O_NONBLOCK;
    if(fcntl(fd,F_SETFL,fd_mod)==-1){printf("set_fd_noblock: fcntl failed\n");return -1;}
    return 0;
}
