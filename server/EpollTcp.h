
#pragma once 
#ifndef _EPOLLTCP_H_
#define _EPOLLTCP_H_

#include <sys/epoll.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "ThreadPool.h"
#include "ThreadPool.cpp"
#include <string>
#include "filOperate.h"

using namespace std;

#define BACKLOG 20
#define MAXEVENT 100
#define POOLMAX  100
#define POOLMIN  100
#define MAXTRANS 30

//本次传输相关的一些数据
//比如文件映射的指针位置 、要接收多少块数据、已经接受了多少块数据
struct st_conn
{
    char filname[FILNAMESIZE]; //文件名
    int  filfd; //文件描述符
    char* p_fil; // 映射在内存的文件指针
    int rev_count; //已经接收的分块数量
    int need_count; //总的需要接收的分块的数量
    int block_size; //分块发送的大小
    int fil_size; //文件大小
};


//封装的EPOLL
class EpollTcp
{
public:

    int accept_sock;
    struct epoll_event event;
    struct epoll_event* events;
    char* port;  
    int efd; //epoll 句柄
    struct sockaddr_in sockaddr_serv;

    vector<struct st_conn*> trans_set;
    //针对trans_set的锁
    pthread_mutex_t  trans_set_lock;

    ThreadPool<struct st_args> thpool=ThreadPool<struct st_args>(POOLMIN,POOLMAX);

public:

    EpollTcp(char *arg);
    EpollTcp()=delete;
    int wait();
    static void  work(void* arg);
    static bool recv_fil_info(EpollTcp* ep,int sockfd);
    static int recv_data( int sockfd,void*  recv_mem,int recv_size);

private:
    int create_bind(char* arg);
    int set_fd_noblock(int fd);
    int registerClient(int clnt_fd);//新的客户端接入注册
    int dealTask(struct epoll_event event);//处理客户端发来的任务
    int dealTransaction(int num);//处理事件
};



struct st_args
{
    EpollTcp* ep;
    struct epoll_event* event; 
};

#endif