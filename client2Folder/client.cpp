/**
    一个多线程发送文件的服务器
*/
// #include "../filOperate.h"
#include "../filUtil/filOperate.h"
#include "client.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <sys/socket.h>

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

    printf("线程个数%d\n",pids.size());
    for(auto pid :pids)
    {
        pthread_join(pid, NULL);
    }
    printf("线程个数%d\n",pids.size());
    // free(mapMem_p) ; 这里不能释放 会出现segment default的bug
    //取消文件映射
    munmap(mapMem_p,filInfo->size);
    free(filInfo) ;
    close(filFd);
}

//初始化服务器地址
inline void Client::initServAddr(const char* ip ,const char* port)
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

//发送数据
//buf:数据
//send_size：数据大小
//closeFd : 发送完了是否等待服务端发来的消息，true为发送完立刻退出 否则等待
int Client::sendData(const void* buf ,  int send_size,bool waitServer)
{
        printf("sendData start\n");
        printf("---------------开始发送-------------\n");
        char* sendBuf = (char*)malloc(send_size+INTSIZE);
        if(sendBuf==nullptr){printf("sendData malloc failed\n");exit(-1);}
        int send_size2 = send_size;

        memset(sendBuf,0,sizeof(sendBuf+INTSIZE));
        memcpy(sendBuf,&send_size2,INTSIZE);
        memcpy(sendBuf+INTSIZE,buf,send_size);

        int data_send_fd = connectServer();
 
        send_size += INTSIZE;
        int totalSend  = 0 ;
        int left_send  = send_size; //还需要发送的数据
        while(totalSend!=send_size)
        {
            int cur_send = send(data_send_fd,sendBuf+totalSend,left_send,0);

            if(cur_send==-1)
            {
              continue;
            }else if(cur_send==0)
            {
                perror("TCP断开\n");
                exit(-1);
            }
            totalSend += cur_send;
            left_send = send_size - totalSend;
        }

        printf("发送了%d个字节\n",totalSend);
        printf("size = %d , type= %d\n",*(int*)(sendBuf),*(sendBuf+INTSIZE));
        //等待服务器端的消息
        if(waitServer)
        {
            int recv_size = -1;
            char buf[INTSIZE*2];
            memset(buf,0,sizeof(buf));
            // for(int i =0 ;i < INTSIZE;i++)
            // {
            //     if(recv(data_send_fd ,buf+i , 1,0 ) == -1)
            //     {
            //         perror("sendData : recv failed\n");
            //         exit(-1);
            //     }
            // }
            if(recvData(data_send_fd, buf, INTSIZE) == 0 )
            {
                printf("传输失败\n");
                exit(-1);
            }
            memcpy(&recv_size , buf , INTSIZE);
            printf("recv_size = %d\n",recv_size);

            int id = -1;

            //接收后面传来的id
            // for(int i =0; i< recv_size;i++)
            // {
            //     if(recv(data_send_fd,buf+INTSIZE+i,1,0) == -1)
            //     {
            //         perror("recv(data_send_fd,buf+INTSIZE+i,1,0) failed\n");
            //         exit(-1);
            //     }   
            // }
            if(recvData(data_send_fd, buf+INTSIZE, INTSIZE) == 0 )
            {
                printf("传输失败\n");
                exit(-1);
            }

            memcpy(&id , buf+INTSIZE , recv_size);
            send_id = id;
            printf("recv id = %d \n",id);
        }

        
        //等待服务端通知该次传输的断开
        char watiBuf[1];
        int notify = -1;
        //printf("send bytes %d ,等待断开通知\n",totalSend);

        while(notify!=0)
        {
            notify = recv(data_send_fd, watiBuf,1, 0);
        }
        close(data_send_fd);
        printf("send bytes %d 得到断开通知，断开\n",totalSend);
        printf("---------------发送结束-------------\n");
        free(sendBuf);
        return totalSend;
}

int Client::sendFileInfo(const struct st_filInfo* filInfo)
{

    printf("################打印filInfo#############\n");
    printf("filname = %s filSize = %d\n",filInfo->filname,filInfo->size);
    //发送文件信息
    char* sendBytes = (char*)malloc(sizeof(struct st_filInfo)+INTSIZE);
    printf("sizeof(struct st_filInfo) = %d\n",sizeof(struct st_filInfo));
    
    memset(sendBytes,0,sizeof(sendBytes));
    int send_type = type_filinfo;
    memcpy(sendBytes,(&send_type),INTSIZE);

    memcpy(sendBytes+INTSIZE,filInfo,sizeof(struct st_filInfo));

    sendData(sendBytes,sizeof(struct st_filInfo)+INTSIZE,true);
    free(sendBytes);
    return 0;
}


//发送文件数据
bool Client::sendFile()
{
    //先分析要分成个多少个大的数据块进行传输
    int fil_size = filInfo->size;   
    int block_cnt = fil_size / BLOCKSIZE;
    
    //先传输前block_cnt块
    //每一次交给一个线程进行传输
    for(int i = 0 ; i < block_cnt ; i++)
    {
        char* block_start = mapMem_p;
        sendBlcok(block_start, BLOCKSIZE, i*BLOCKSIZE);
    }

    //如果fil_size / BLOCKSIZE不是整数 还要再传输剩余的一块
    if(fil_size % BLOCKSIZE != 0)
    {
       
        char* block_start = mapMem_p;
        sendBlcok(block_start, (fil_size % BLOCKSIZE), block_cnt*BLOCKSIZE);
    }
    return true;

}

//将文件数据部分传输给服务器端
void Client::sendBlockData(const int sockfd,const char*  mp , const int offset ,const int send_size,const int id)
{
    struct st_head* head  = new st_head();
    head->id = id ;
    head->offset = offset;
    head->size = send_size;

    int left_send = INTSIZE;
    
    int type = type_fildata ; 
    //先发送后面数据的长度
    int size = INTSIZE+head->size+INTSIZE;
    Csend(sockfd, (char*)&size, INTSIZE); 
   
     //发送type
    Csend(sockfd, (char*)&type, INTSIZE);
    
    //在发送头部
    Csend(sockfd, (char*)head,sizeof(struct st_head));


    Csend(sockfd, mp, send_size);
    

    char ret[1];
    //等待服务端断开连接
    while(true)
    {
        int n = recv(sockfd ,ret  , 1 ,0  );
        if(n==0)break;
    }
    printf("发送完%d , %d 传输断开\n",send_size,sockfd);
    close(sockfd);
    delete head;
}

void Client::Csend(const int sockfd,const char* buf  , const int send_size )
{
     //发送数据
    int total_send = 0;
    int left_send = send_size;
    while(total_send!=send_size)
    {
        int n = send(sockfd , buf+total_send,left_send,0);
        if(n==-1)continue;
        if(n==0)
        {
            printf("connection disconnect \n");
            exit(-1);
        }
        total_send+=n;
        left_send = send_size - total_send;
        printf("send n = %d this time \n",n);
    }
}


//发送一个文件数据块
//需要将文件数据块分成若干更小块进行发送
bool Client::sendBlcok(const char* send_start ,const int send_size,const int offset)
{
    int send_cnt = send_size / SEND_RECV_SIZE;
    for(int i =0; i < send_cnt;i++)
    {
        //建立与服务端的连接
        int sockfd = connectServer();
        if(sockfd == -1){perror("connect failed\n");exit(-1);}

        //创建一个线程发送
        pids.push_back(0);

        th_Args.push_back(new st_cl_sendData_Args());
        //初始化head
        th_Args.back()->head->id = send_id;
        th_Args.back()->head->offset =  offset + i*SEND_RECV_SIZE;
        th_Args.back()->head->size = SEND_RECV_SIZE;
        th_Args.back()->p = mapMem_p;
        th_Args.back()->sockfd = sockfd;

        printf("交给一个线程%d\n", i);
        pthread_create(&pids[pids.size()-1], NULL,threadSend , th_Args.back());
        // sendData(send_buf,SEND_RECV_SIZE+sizeof(struct st_head));
        
    }

    /**
    如果还有剩余 就再发送一次
    */
    if(send_size % SEND_RECV_SIZE !=0)
    {
         //建立与服务端的连接
        int sockfd = connectServer();
        if(sockfd == -1){perror("connect failed\n");exit(-1);}

        
        //交给线程池发送
        // thpool.addTask(threadSend,arg);
        pids.push_back(0);

        th_Args.push_back(new st_cl_sendData_Args());
        //初始化head
        th_Args.back()->head->id = send_id;
        th_Args.back()->head->offset =  offset + send_cnt*SEND_RECV_SIZE;
        th_Args.back()->head->size = send_size % SEND_RECV_SIZE;
        th_Args.back()->p = mapMem_p;
        th_Args.back()->sockfd = sockfd;

        pthread_create(&pids[pids.size()-1], NULL,threadSend ,th_Args.back());
    }
    //free(send_buf);
    return true;
}

void* Client::threadSend(void* arg)
{
    
    struct  st_cl_sendData_Args* send_arg = (struct  st_cl_sendData_Args*)(arg);
    sendBlockData(send_arg->sockfd,send_arg->p,send_arg->head->offset,send_arg->head->size,send_arg->head->id);


    return NULL;
}