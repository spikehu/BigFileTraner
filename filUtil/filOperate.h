/*
*   文件操作头文件 
    包括发送 接收 创建  以及相关结构体
*/

#pragma once 
#ifndef _FILOPERATE_H_
#define _FILOPERATE_H_
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "stdio.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FILNAMESIZE 100 //文件名最大长度
#define SEND_RECV_SIZE 65536 //每次接收和发送的字节数量 64k 小于接收缓冲区的大小
#define BLOCKSIZE 536870912 //对文件的分块大小 这是512M 客户端进行划分
#define INTSIZE 4
#define type_filinfo -1  //发送文件信息前先发送-1表示传输的是文件信息
#define type_fildata  1  //发送文件内容前发送1 表示传输的是数据


struct st_filInfo
{
    int size ; //文件大小
    char filname[FILNAMESIZE];
};

struct st_head
{
    int id ; //自己所属于的文件
    int offset ;//偏移量
    int size ; //这次传输数据块的大小
};






//filname:文件名字
bool creat_fil(struct st_filInfo* filInfo);

//filname:文件名字
//size: 文件大小
bool creat_fil(const char* filname ,const int filsize );

//第一次建立连接需要接受的信息
bool recvFilinfo(int sockfd , struct filInfo* st_filInfo);

//接收传输的文件数据内容
//sockfd：套接字
//st_head:需要发送的头部
bool recvFilData(int sockfd , struct filData* st_head);

//初始化文件信息
//filName:文件名
struct st_filInfo* initFileInfo(const char* filName );

//将文件映射到内存上
//filInfo：包含文件信息的结构体
char* myMmap(struct st_filInfo* filInfo);

char* myMmap(const char* filname , const int size);


#endif