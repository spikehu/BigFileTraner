
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


#define BACKLOG 20
#define MAXEVENT 100
#define POOLMAX  100
#define POOLMIN  100

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
    ThreadPool<struct args> thpool=ThreadPool<struct args>(POOLMIN,POOLMAX);

public:

    EpollTcp(char *arg);
    EpollTcp()=delete;
    int wait();
    static void  mySend(void* arg);
   

private:
    int create_bind(char* arg);
    int set_fd_noblock(int fd);
    int registerClient(int clnt_fd);//新的客户端接入注册
    int dealTask(struct epoll_event event);//处理客户端发来的任务
    int dealTransaction(int num);//处理事件
};

#endif