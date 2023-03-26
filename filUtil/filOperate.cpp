#include "filOperate.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

//filname:文件名字
bool  creat_fil(struct st_filInfo* filInfo)
{
    int fd = open(filInfo->filname,O_CREAT|O_RDWR); 
    if(fd ==-1)
    {
        printf("creat_fil failed\n");
        return FILNAMESIZE;
    }
    //将文件变大
    lseek(fd,filInfo->size-1, SEEK_END);
    write(fd,"",1);
    close(fd);
    return true;
}


//filname:文件名字
//size: 文件大小
bool creat_fil(const char* filname ,const int filsize )
{
    int fd = open(filname,O_CREAT|O_RDWR); 
    if(fd ==-1)
    {
        printf("creat_fil failed\n");
        return FILNAMESIZE;
    }
    //将文件变大
    lseek(fd,filsize-1, SEEK_END);
    write(fd,"",1);
    close(fd);
    return true;    
}

//第一次建立连接需要接受的信息
bool recvFilinfo(int sockfd , struct filInfo* st_filInfo);

//接收传输的文件数据内容
//sockfd：套接字
//st_head:需要发送的头部
bool recvFilData(int sockfd , struct filData* st_head);

//初始化文件信息
//filName:文件名
struct st_filInfo* initFileInfo(const char* filName )
{
    struct st_filInfo* filInfo = NULL;
    int fd = open(filName,O_RDWR);
    if(fd == -1)
    {
        perror(filName);
        close(fd);
        return NULL;
    }

    filInfo = (struct st_filInfo*)malloc(sizeof(struct st_filInfo));
    memset(filInfo,0,sizeof(struct st_filInfo));

    struct stat statbuf;
    stat(filName,&statbuf);
    filInfo->size = statbuf.st_size;
    memcpy( filInfo->filname ,filName,strlen(filName));
    close(fd);
    return filInfo;
}


//将文件映射到内存上
//filInfo：包含文件信息的结构体
char* myMmap(struct st_filInfo* filInfo)
{
    //打开文件
    int fd = open(filInfo->filname,O_RDWR);
    if(fd == -1)
    {
        perror(filInfo->filname);
        return NULL;
    }

    char* p = (char*)mmap(NULL,filInfo->size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(p == MAP_FAILED)
    {
        printf("mmap failed\n");
        close(fd);
        return NULL;
    }
    close(fd);
    return p;
}
char* myMmap(const char* filname , const int size)
{
     //打开文件
    int fd = open(filname,O_RDWR);
    if(fd == -1)
    {
        perror(filname);
        return NULL;
    }

    char* p = (char*)mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(p == MAP_FAILED)
    {
        printf("mmap failed\n");
        close(fd);
        return NULL;
    }
    memset(p,0,size);
    close(fd);
    return p;
}


//向sockf接收buf
int recvData(int sockfd , char* buf , int recv_size)
{   
    memset(buf,0, recv_size);
    int total_recv = 0; 
    int left_recv = recv_size;
    while(total_recv!=recv_size)
    {
        int cur_recv = recv(sockfd ,buf+total_recv , left_recv, 0);
        if(cur_recv==-1)continue;
        if(cur_recv==0)
        {
           perror("传输异常断开\n");
           exit(-1);
        }else
        {
            total_recv+=cur_recv;
            left_recv  = recv_size - total_recv;
        }
    }
    return total_recv;
}
