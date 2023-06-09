/**
    多线程发送文件的客户端
    使用方式: ./client [filname]
*/
#pragma once
#ifndef _CLIENT_H_
#define _CLIENT_H_

#include "ThreadPool.h"
#include "ThreadPool.cpp"
#include "../filUtil/filOperate.h"
#include <sys/socket.h>
#include <arpa/inet.h>

#define POOLMIN 100
#define POOLMAX 200

const int MAXSEND = SEND_RECV_SIZE+100;



//传给客户端发送数据的线程 函数的参数结构体
struct st_cl_sendData_Args
{
    char* p ;//映射的起始指针
    struct st_head* head;
};

//传给客户端发送文件信息的线程 函数的参数结构体
struct st_cl_sendFilInfo_Args
{
    struct st_filInfo* filInfo;
};

union clnt_send_args
{
    struct st_cl_sendFilInfo_Args* fil_args;
    struct st_cl_sendData_Args* data_args;
};





class Client
{
public:
    Client()=delete;
    Client(char* ip , char* port ,char* fil); //需要传入ip地址 端口号 文件名
    bool sendFile(const char* filname);//文件发送函数
   ~Client();
    bool sendFile();
private:
 ThreadPool<union clnt_send_args> thpool=ThreadPool<union clnt_send_args>(POOLMIN,POOLMAX);
public:

    struct sockaddr_in serv_addr;
    int filFd; // 文件描述符
    char* mapMem_p;  //文件被隐射的地址
    struct st_filInfo* filInfo; //包含文件信息的结构体
    int send_id ;//服务端发来表示数据块标识的id

private:
    
    void* sendFileBlock(const void* buf,const int size); //发送文件数据块
    bool  sendFilInfo(struct filInfo* filInfo);//发送文件信息
    void initServAddr(const char* ip ,const char* port);//初始化地址
    int connectServer(); //连接到服务器
    int sendFileInfo(const struct st_filInfo* filInfo);
    int sendData(const void* buf , int send_size,bool waitServer=false);
   
    bool sendBlcok(const char* send_start ,const int send_size,const int offset);//发送一个文件数据块
    static void threadSend(void* arg);

};

struct  st_thread_send_arg
{
    int send_size;
    int id;
    char send_buf[MAXSEND];
    Client** clt;
   
};
#endif
