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
    sockfd = socket(AF_INET ,SOCK_STREAM,0);
    if(connectServer() == false)
    {
        printf("connectServer failed\n");
        exit(-1);
    }
    
    printf("connect to %s:%s\n",ip,port);

    //连接成功 进行文件映射
    mapMem_p = myMmap(filInfo);
    if(mapMem_p == NULL)
    {
        printf("myMmap failed\n");
        exit(-1);
    }
    printf("##############----map file %s to memory success --############### \n",fil);

    printf("filsize:%d\n",strlen(mapMem_p));
    printf("%c\n",mapMem_p[4]);

    for(int i =0 ;i <= strlen(mapMem_p);i++)
    {
        printf("index = %d\n",i);
        printf("%c\n",mapMem_p[i]);
    }
    // //打印一下映射过去的内容
    // printf("%s",mapMem_p);

}
Client::~Client()
{
    delete mapMem_p;
    delete filInfo;
}

//初始化服务器地址
inline void Client::initServAddr(char* ip ,char* port)
{
    serv_addr.sin_addr.s_addr  = inet_addr(ip);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
}

//连接到服务器端
bool Client::connectServer()
{
    socklen_t socklen = sizeof(serv_addr);
    if(connect(sockfd,(struct sockaddr*)&serv_addr,socklen)==-1)
    {
        perror("connect failed:");
        return false;
    }

    
    return true;
}
