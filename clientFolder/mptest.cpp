#include <unistd.h>
#include <string>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>


using namespace std;

//创建一个多线程发送文件的客户端

struct sockaddr_in serv_addr;

void initServAddr(char* ip ,char* port);

//传入参数：文件名 服务端IP  服务端端口
int main(int argc, char* argv[])
{       
    if(argc != 4)
    {
        printf("Using:./mptest [fileName] [IP] [PORT]\n");
        return -1;
    }


    //进行客户端的连接
    int cl_sock =  socket(AF_INET , SOCK_STREAM , 0);
    if(cl_sock == -1){printf("socket() faield\n");return -1;}
    
    initServAddr(argv[2],argv[3]);
    socklen_t seraddr_size = sizeof(serv_addr);
    if(connect(cl_sock,(struct sockaddr*)&serv_addr,seraddr_size)==-1)
    {
        printf("connect failed\n");
        return -1;
    }

    //多线程发送文件内容
    

    return 0;
}


//对serv_addr进行定义
void initServAddr(char* ip ,char* port)
{   
    serv_addr.sin_addr.s_addr  = inet_addr(ip);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
}