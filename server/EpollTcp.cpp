
#include "./EpollTcp.h"
#include "ThreadPool.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <typeinfo>
#include <unistd.h>
using namespace std;

string loadFolder = "upload/";

int fil_info_size = sizeof(struct st_filInfo);

void* EpollTcp::work(void* arg)
{
    printf(" 线程 %ld 进入work\n",pthread_self());

    struct st_args* send_arg = (struct st_args*)arg;
    //接收数据

    int index = 0;
    struct st_head* fil_head = new st_head();
    char* fil_recv = (char*)malloc(SEND_RECV_SIZE);
    struct st_conn* conn = nullptr;

    char* buf_fil_head = (char*)malloc(sizeof(struct st_head));
    memset(buf_fil_head, 0, sizeof(struct st_head));

    // printf("开始接收文件数据\n");
    //1.接受文件数据部分
    //将数据部门映射到对应的内存中
    //先读取头部



    if(fil_head == nullptr)
    {
        printf("new st_head failed\n");
        exit(-1);
    }

    
    if(recvData(send_arg->event->data.fd,buf_fil_head,sizeof(struct st_head))!=sizeof(struct st_head))
    {
        printf("recv file data faield.\n");
        pthread_exit(NULL);
    }
    memcpy(fil_head , buf_fil_head , sizeof(struct st_head));

    printf("头部读取完成 \n");
    printf("读取数据部分\n");


    //打印头部
    printf("-------------打印头部信息-----");
    printf("head->size = %d head->offset = %d  head->id = %d\n",fil_head->size,fil_head->offset,fil_head->id);

    //接收文件内容
    if(recvData(send_arg->event->data.fd,(char*)fil_recv,fil_head->size) != fil_head->size)
    {
        printf("receive file failed\n");
        pthread_exit(NULL);
    }

    printf("读取数据部分完成\n");
    printf("---------开始映射 id = %d-----------\n",fil_head->id);

    // 映射到对应的内存中
    conn = send_arg->ep->trans_set[fil_head->id];
    memcpy(conn->p_fil+fil_head->offset,fil_recv,fil_head->size);
    printf("映射完成\n");
    //拷贝完成 收到的块数加1
    pthread_mutex_lock(&send_arg->ep->trans_set_lock);
    conn->recv_count+=1;
    printf("进入锁\n");
    pthread_mutex_unlock(&send_arg->ep->trans_set_lock);
    printf("conn->recv_count = %d\n",conn->recv_count);
    printf("conn->need_count = %d \n",conn->need_count);
    if(conn->recv_count == conn->need_count)
    {
        printf("接受完毕\n");
        //取消映射 并从vector中删除该conn
        munmap(conn->p_fil,conn->fil_size);
        send_arg->ep->trans_set.erase(send_arg->ep->trans_set.begin()+fil_head->id);
        free(conn);
        // exit(1);
    }
    //接收完成一次就要将该次传输的注册事件删除
    printf("接收完一个块\n");
    //通知客户端断开这次连接
    printf("通知断开\n");
    delEpollEvent(send_arg->ep,send_arg->event);
    free(fil_recv);
    free(fil_head);
    printf(" 线程 %ld 离开work\n",pthread_self());
    return nullptr;
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
        // printf("-----wating num  ----------\n");
        //开始阻塞等待
        int num = epoll_wait(efd,events,MAXEVENT,0);
        // printf("-----wating num = %d ----------\n",num);
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
                }else if(events[i].events & EPOLLIN)//客户端发来数据
                {
                     epoll_ctl(efd, EPOLL_CTL_DEL, events[i].data.fd, &events[i]);
                    //将任务加入工作队列
                    dealTask(&events[i]);
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

int EpollTcp::dealTask(struct epoll_event *event)
{


        //取出数据大小部分 和 传输类型部分
        int size;
        int type;
        char buf[INTSIZE];
        memset(buf, 0,INTSIZE);
        int ret =  recvData(event->data.fd,buf, INTSIZE);
        if(ret == 0 ) return 0;
        memcpy(&size , buf , INTSIZE);

        memset(buf, 0,INTSIZE);
        ret =   recvData(event->data.fd, buf, INTSIZE);
        if(ret == 0)return 0;
        memcpy(&type , buf , INTSIZE);
        
        printf("recv type = %d  size = %d \n",type,size);


        if(type == type_filinfo)
        {
            //建立新的文件传输
            int  index = recv_fil_info(event->data.fd);
            if(index==-1)
            {
                printf("recv_fil_info failed\n");
                exit(-1);
            }
            
            //向客户端发送该传输在控制数组中下标
            if(sendData(event->data.fd,&index,sizeof(index))==false)
            {
                printf("index send failed\n");
                exit(-1);
            }
            delEpollEvent(this, event);
            printf("返回conn 完成\n");
        }else if(type == type_fildata)
        {
            struct st_args* arg = new st_args();
            arg->event = event;
            arg->ep = this;
            thpool.addTask(work,(void*)arg);
        }else
        {
            printf("error type =  %d\n",type);
            exit(-1);
        }
    return 0;
}

int EpollTcp::recv_fil_info( int sockfd)
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
    if(recvData(sockfd,recv_buf,fil_info_size)!=fil_info_size)
    {
        printf("need %d bytes , recv faild\n",fil_info_size);
        exit(-1);
    }
    
    memcpy(filInfo,recv_buf,fil_info_size);
    printf("filname =%s , filsize = %d\n",filInfo->filname,filInfo->size);

    trans_set.push_back(conn);
    
    pthread_mutex_lock(&(trans_set_lock));
    //本次文件传输的控制块的在trans_Set的下标
    set_index = trans_set.size() -1; 
    pthread_mutex_unlock(&(trans_set_lock));
    

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
    close(event->data.fd);
    epoll_ctl(ep->efd,EPOLL_CTL_DEL,event->data.fd,event);
   
}