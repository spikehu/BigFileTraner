
#include "EpollTcp.h"
#include "ThreadPool.h"
using namespace std;

int fil_info_size = sizeof(struct st_filInfo);

void EpollTcp::work(void* arg)
{
    struct args* send_arg = (struct args*)arg;
    //接收数据

    //将前4个字节取出 表示后面数据的长度

    char size[INTSIZE]; // 数据部分有多少个字节
    for(int i =0;i<INTSIZE;i++)
    {
        int n = recv(send_arg->event->data.fd,size+i,1,0);
        if(n==-1){printf("work:: recv failed\n");exit(-1);}
    }

    char  recv_type[INTSIZE]; //接收的类型是文件信息 还是文件数据
    for(int i =0 ;i < INTSIZE ;i++)
    {
        if(recv(send_arg->event->data.fd,recv_type+i,1,0)!=1)
        {
            printf("work recv failed\n");
            exit(-1);
        }
    }

    //根据类型决定是接收文件数据 还是接受文件信息

    printf("要有%d个字节传来\n",*(int*)size);

    int type = -2 ;
    memcpy(&type,recv_type,INTSIZE);
    printf("type == %d\n",type);


    char recv_buf[fil_info_size];
    struct st_filInfo* fil = (struct st_filInfo*)malloc(sizeof(struct st_filInfo));

    
    //接收文件信息并且打印
    for(int i =0 ;i < fil_info_size;i++)
    {
        if(recv(send_arg->event->data.fd,recv_buf+i,1,0) ==-1)
        {
            perror("recv struct st_filInfo failed\n");
            exit(-1);
        }
    }
    memcpy(fil,recv_buf,fil_info_size);
    printf("接收完毕\n");
    //打印一下
    printf("filname = %s , filsize = %d\n",fil->filname,fil->size);
    free(fil);
    
}

EpollTcp::EpollTcp(char *arg):port(arg)
{
   // thpool(3,10)
    this->accept_sock = create_bind(port);
   if(set_fd_noblock(accept_sock) == -1){exit(-1);}
     //创建句柄
    efd = epoll_create(1);
    if(efd == -1){printf("epoll_create failed\n");exit(-1);}

    //初始化监听端口的事件
    event.data.fd = accept_sock;
    event.events  = EPOLLIN|EPOLLET;
    //注册
    if(epoll_ctl(efd,EPOLL_CTL_ADD,accept_sock,&event) == -1){printf("epoll_ctl failed\n");exit(-1);}
    events = (struct epoll_event*)malloc(MAXEVENT*sizeof(struct epoll_event));
    memset(events,0,sizeof(events));
}

int EpollTcp::create_bind(char* arg)
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

int EpollTcp::set_fd_noblock(int fd)
{
    int fd_mod = fcntl(fd , F_GETFL,0);
    if(fd_mod == -1){printf("set_fd_noblock: fcntl() failed\n");return -1;}

    //设置为非阻塞
    fd_mod |= O_NONBLOCK;
    if(fcntl(fd,F_SETFL,fd_mod)==-1){printf("set_fd_noblock: fcntl failed\n");return -1;}
    return 0;
}
int EpollTcp::wait()
{
    while(true)
    {
        //开始阻塞等待
        int num = epoll_wait(efd,events,MAXEVENT,-1);
        if(num == -1){printf("epool_wait failed\n"); exit(-1);}
        if(dealTransaction(num) == -1){printf("dealTransaction failed\n");return -1;}
    }
    return 0;
}

int EpollTcp::dealTransaction(int num)
{
     //有事件发生
        for(int i = 0;i < num ; i++)
        {
            if(events[i].events & EPOLLIN) 
            {
                if(events[i].data.fd == accept_sock) // 有客户端连接
                {
                    int clnt_fd = accept(accept_sock,NULL,0);
                    //新的客户端接入注册
                    if(registerClient(clnt_fd) == -1)
                    {
                        printf("registerClient failed\n");
                        return -1;   
                    }
                }else //客户端发来数据
                {
                    printf("dealTask %d \n",num);
                    //将任务加入工作队列
                    dealTask(events[i]);
                }
            }
        }
        return 0;
}
int EpollTcp::registerClient(int clnt_fd)
{
    set_fd_noblock(clnt_fd); // 设置为非阻塞

    printf("客户端%ld接入\n",clnt_fd);
    if(clnt_fd == -1 ){printf("accept failed\n");return -1;}
    //进行注册
    event.data.fd = clnt_fd;
    event.events  = EPOLLIN |EPOLLET;
    if(epoll_ctl(efd ,EPOLL_CTL_ADD, clnt_fd, &event)==-1){printf("register failed\n");return -1;}
    return 0;
}

int EpollTcp::dealTask(struct epoll_event event)
{

        struct args* arg = new args();
        arg->fd = event.data.fd;
        arg->event = &event;
        thpool.addTask(work,(void*)arg);
            //客户端发来消息
            //接收并且发回去
            //char buf[100];
            // memset(buf,0,sizeof(buf));
            // int clnt_fd = event.data.fd;
            // int n = recv(clnt_fd,buf,100,0);
            // if(n==-1)
            // {
            //         if(errno == EAGAIN || errno == EWOULDBLOCK)
            //         {
            //             //读取完毕
            //             break;
            //         }
            //         close(clnt_fd);
            //         break;
            // }
            // else if(n==0)
            // {
            //     printf("%ld 断开连接\n",clnt_fd);
            //     close(clnt_fd);
            //     break;
            // }else //将读取到的字符串发送出去
            // {
            //       // send(clnt_fd,buf,n,0);
            //         struct args* arg = new args();
            //         arg->fd = clnt_fd;
            //         memcpy( arg->buf,buf,sizeof(buf));
            //         arg->n = n;
                  
            //         thpool.addTask(mySend,(void*)arg);
            // }


    return 0;
}

