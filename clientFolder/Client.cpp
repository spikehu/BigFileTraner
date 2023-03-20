/**
    一个多线程发送文件的服务器
*/
#include "filOperate.h"
#include "Client.h"

Client::Client(char* ip , char* port ,char* fil) //需要传入ip地址 端口号 文件名
{
    //初始化文件信息
    filInfo = initFileInfo(fil);
    initServAddr(ip,port);
    //进行文件映射
    mapMem_p = myMmap(filInfo);
    if(mapMem_p == NULL)
    {
        printf("myMmap failed\n");
        exit(-1);
    }
    sendFileInfo(filInfo);

}
Client::~Client()
{
    // free(mapMem_p) ; 这里不能释放 会出现segment default的bug
    //取消文件映射
    munmap(mapMem_p,filInfo->size);
    free(filInfo) ;
    close(filFd);
}

//初始化服务器地址
inline void Client::initServAddr(char* ip ,char* port)
{
    serv_addr.sin_addr.s_addr  = inet_addr(ip);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
}

//连接到服务器端
//成功返回套接字
int Client::connectServer()
{
    int sockfd  = socket(AF_INET,SOCK_STREAM,0);
    socklen_t socklen = sizeof(serv_addr);
    if(connect(sockfd,(struct sockaddr*)&serv_addr,socklen)==-1)
    {
        perror("connect failed:");
        return -1;
    }
    return sockfd;
}

//发送文件数据
int Client::sendData(void* buf , int send_size)
{
        char* sendBuf = (char*)malloc(send_size+INTSIZE);
        memset(sendBuf,0,sizeof(sendBuf+INTSIZE));

        int* int_size = new int(send_size);
        memcpy(sendBuf,int_size,INTSIZE);
        memcpy(sendBuf+INTSIZE,buf,send_size);

        int data_send_fd = connectServer();

        send_size += INTSIZE;
        int totalSend  = 0 ;
        int left_send  = send_size; //还需要发送的数据
        while(totalSend!=send_size)
        {
            int cur_send = send(data_send_fd,sendBuf,left_send,0);
            if(cur_send==-1)
            {
                printf("发送失败\n");
                exit(-1);
            }
            totalSend += cur_send;
            left_send = send_size - totalSend;
        }
        printf("发送了%d个字节\n",totalSend);
        delete int_size;
        close(data_send_fd);
        return totalSend;
}

int Client::sendFileInfo(struct st_filInfo* filInfo)
{
    //发送文件信息
    char* sendBytes = (char*)malloc(sizeof(struct st_filInfo)+INTSIZE);
    printf("sizeof(struct st_filInfo) = %d\n",sizeof(struct st_filInfo));
    
    memset(sendBytes,0,sizeof(sendBytes));
    int send_type = type_filinfo;
    memcpy(sendBytes,(&send_type),INTSIZE);

    memcpy(sendBytes+INTSIZE,filInfo,sizeof(struct st_filInfo));

    sendData(sendBytes,sizeof(struct st_filInfo)+INTSIZE);
    free(sendBytes);
}