
#include "./EpollTcp.h"
#include "ThreadPool.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
using namespace std;

string loadFolder = "upload/";

int fil_info_size = sizeof(struct st_filInfo);

void EpollTcp::work(void* arg)
{
    struct st_args* send_arg = (struct st_args*)arg;
    //接收数据

    //将前4个字节取出 表示后面数据的长度

    char size[INTSIZE]; // 数据部分有多少个字节
    // int i = 0;
    // while(i!=INTSIZE)
    // {
    //     int n = recv(send_arg->event->data.fd,size+i,1,0);
    //     if(n==1)i++;
    // }
    for(int i =0;i<INTSIZE;i++)
    {
        int n = recv(send_arg->event->data.fd,size+i,1,0);
        if(n==-1)
        {
            //printf("work:: recv failed\n");exit(-1);
             close(send_arg->event->data.fd);
             pthread_exit(0);
        }
        else if(n==0)
        {
             printf("client disconnect\n");
             return ;
        }
    }

    char  recv_type[INTSIZE]; //接收的类型是文件信息 还是文件数据
    // i=0;
    // while(i!=INTSIZE)
    // {
    //     int n = recv(send_arg->event->data.fd,recv_type+i,1,0);
    //     if(n==1)i++;
    // }
    for(int i =0 ;i < INTSIZE ;i++)
    {
        int recv_ret = recv(send_arg->event->data.fd,recv_type+i,1,0);
        if(recv_ret==-1)
        {
            printf("work recv failed\n");
            close(send_arg->event->data.fd);
            pthread_exit(0);
           // exit(-1);
        }else if(recv_ret ==0)
        {
            printf("client disconnect\n");
            return ;
        }
    }
    
    int recv_size = *(int*)size;
    int type = -2 ;
    memcpy(&type,recv_type,INTSIZE);
    // printf("type == %d\n",type);
    int index = 0;
    struct st_head* fil_head = nullptr;
    char* fil_recv = (char*)malloc(SEND_RECV_SIZE);
    struct st_conn* conn = nullptr;

    //根据类型决定是接收文件数据 还是接受文件信息
    switch(type)
    {
        case type_filinfo:
            //1.接受文件信息 
            //2.在服务端创建文件
            //3.将创建的文件映射到内存中
            //4.并且在相应的传输控制数组中初始化相关参数 
            //5.向客户端发送该传输在控制数组中下标
            index = recv_fil_info(send_arg->ep,send_arg->event->data.fd);
            if(index==-1)
            {
                printf("recv_fil_info failed\n");
                exit(-1);
            }
            
            //向客户端发送该传输在控制数组中下标
            if(sendData(send_arg->event->data.fd,&index,sizeof(index))==false)
            {
                printf("index send failed\n");
                exit(-1);
            }

            //接收完成一次就要
            //关闭套接字以及该次传输的注册事件删除
            break;

        case type_fildata:
               // printf("开始接收文件数据\n");
            //1.接受文件数据部分
            //将数据部门映射到对应的内存中

            //先读取头部
             fil_head = new  st_head();
             if(fil_head == nullptr)
             {
                printf("new st_head failed\n");
                exit(-1);
             }

            if(recv_data(send_arg->event->data.fd,fil_head,sizeof(struct st_head))!=sizeof(struct st_head))
            {
                printf("recv file data faield.\n");
                exit(-1);
            }

            //打印一下文件头部信息
            // printf("st_head: id = %d\n",fil_head->id);
            // printf("st_head: offset = %d\n",fil_head->offset);
            // printf("st_head: size = %d\n",fil_head->size);

            //接收文件内容
            if(recv_data(send_arg->event->data.fd,fil_recv,fil_head->size) != fil_head->size)
            {
                printf("receive file failed\n");
                exit(-1);
            }

            // 映射到对应的内存中
            conn = send_arg->ep->trans_set[fil_head->id];
            memcpy(conn->p_fil+fil_head->offset,fil_recv,fil_head->size);
            
            //拷贝完成 收到的块数加1
            pthread_mutex_lock(&send_arg->ep->trans_set_lock);
            printf("lock\n");
            conn->recv_count+=1;
            pthread_mutex_unlock(&send_arg->ep->trans_set_lock);
            printf("unlock\n");
            printf("conn->recv_count = %d\n",conn->recv_count);
            if(conn->recv_count == conn->need_count)
            {
                printf("接受完毕\n");
                //取消映射 并从vector中删除该conn
                munmap(conn->p_fil,conn->fil_size);
                send_arg->ep->trans_set.erase(send_arg->ep->trans_set.begin()+fil_head->id);
                free(conn);
            }
            //接收完成一次就要将该次传输的注册事件删除
            break;
        default:
            printf("erro: unkown type\n");
            break; ;
    }
    //通知客户端断开这次连接
   // printf("通知断开\n");
    close(send_arg->event->data.fd);
    delEpollEvent(send_arg->ep,send_arg->event);
    free(fil_recv);
    free(fil_head);
    
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

    //初始化锁
    pthread_mutex_init(&trans_set_lock,NULL);
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

    // printf("客户端%ld接入\n",clnt_fd);
    if(clnt_fd == -1 ){printf("accept failed\n");return -1;}
    //进行注册
    event.data.fd = clnt_fd;
    event.events  = EPOLLIN |EPOLLET;
    if(epoll_ctl(efd ,EPOLL_CTL_ADD, clnt_fd, &event)==-1){printf("register failed\n");return -1;}
    return 0;
}

int EpollTcp::dealTask(struct epoll_event event)
{

        struct st_args* arg = new st_args();
        arg->event = &event;
        arg->ep = this;
        thpool.addTask(work,(void*)arg);

    return 0;
}

int EpollTcp::recv_fil_info(EpollTcp* ep , int sockfd)
{
    int set_index = -1;
    struct st_conn* conn = (struct st_conn*)malloc(sizeof(struct st_conn)); 
    if(conn == nullptr)
    {
        perror("recv_fil_info :new st_conn failed\n");
        exit(-1);
    }
    memset(conn,0,sizeof(struct st_conn));

    struct st_filInfo *filInfo  =(struct st_filInfo*)malloc(fil_info_size);
    if(filInfo == nullptr)
    {
        printf("malloc filInfo failed\n");
        exit(-1);
    }
    memset(filInfo,0,fil_info_size);

    char recv_buf[fil_info_size];

    memset(recv_buf,0,fil_info_size);
    if(recv_data(sockfd,recv_buf,fil_info_size)!=fil_info_size)
    {
        printf("need %d bytes , recv faild\n",fil_info_size);
        exit(-1);
    }
    
    memcpy(filInfo,recv_buf,fil_info_size);
    printf("filname =%s , filsize = %d\n",filInfo->filname,filInfo->size);

    ep->trans_set.push_back(conn);
    
    pthread_mutex_lock(&(ep->trans_set_lock));
    //本次文件传输的控制块的在trans_Set的下标
    set_index = ep->trans_set.size() -1; 
    pthread_mutex_unlock(&(ep->trans_set_lock));
    

    //在服务端创建相应的文件
    char filpath[FILNAMESIZE+loadFolder.size()];
    memset(filpath,0,sizeof(filpath));
    memcpy(filpath,loadFolder.c_str(),loadFolder.size());
    memcpy(filpath+loadFolder.size(),filInfo->filname,FILNAMESIZE);
    creat_fil(filpath,filInfo->size);

    //初始化conn
    if(initStConn(conn, filpath ,filInfo->size) == false)
    {
        printf("initStConn failed\n");
        exit(-1);
    }

    free(filInfo);
    return set_index;
}

int EpollTcp::recv_data(int sockfd ,void*  recv_mem,int recv_size)
{
    int total_recv = 0;
    int left_recv  = recv_size;
    while(total_recv!= recv_size)
    {
        int n  = recv(sockfd,recv_mem ,left_recv,0);
        if(n == -1)
        {
            perror("recv_data failed\n");
            exit(-1);
        }
        if(n == 0 )
        {
            printf("in recv_date client disconnect \n");
            exit(-1);
        }
        total_recv += n;
        left_recv  = recv_size - total_recv ;
    }
    return total_recv;
}

//初始化struct st_conn
//filpath:文件
//filsize : 文件大小
bool EpollTcp::initStConn( struct st_conn* conn,const char* filpath ,const int filsize)
{
    memcpy(conn->filname , filpath , strlen(filpath));
    
    if(strcmp(conn->filname , filpath)!=0)
    {
        printf("initStConn :: memcpy failed\n");
        return false;
    }
    //conn->filfd = open(conn->filname ,O_RDWR);
    //if(conn->filfd == -1){printf("initStConn : open() failed\n");exit(-1);}
    conn->fil_size = filsize;
    conn->recv_count = 0;
    conn->p_fil  = myMmap(filpath,filsize);
    conn->block_size = SEND_RECV_SIZE;

    conn->need_count = (conn->fil_size / conn->block_size);
    if( (conn->fil_size % conn->block_size) != 0 )
    {
        conn->need_count  += 1;
    }
    printf("conn->need_count = %d\n",conn->need_count);
    return true;
}

//将数据发送给对端
//sockfd:套接字
//data :待发送的数据
//size: 数据字节大小
bool EpollTcp::sendData(int sockfd , const void* data ,int data_size )
{
        int send_size = data_size;
        char* send_buf = (char*)malloc(sizeof(int)+data_size);
        memset(send_buf,0,sizeof(send_buf));

        memcpy(send_buf,&send_size,sizeof(send_size));
        memcpy(send_buf+sizeof(send_size) , data , data_size);

        int need_send  = data_size + sizeof(int);
        int sent_bytes = 0 ;
        while(need_send!=0)
        {
            sent_bytes = send(sockfd , send_buf ,need_send , 0);
            if(sent_bytes == -1)
            {
                perror("send failed\n");
                return false;
            }
            need_send -= sent_bytes;
        }
    return true;
}

//当一个TCP连接断开时 关闭套接字 将事件从epoll中删除
void EpollTcp::delEpollEvent(EpollTcp* ep,struct epoll_event* event)
{
    epoll_ctl(ep->efd,EPOLL_CTL_DEL,event->data.fd,event);
    close(event->data.fd);
}
