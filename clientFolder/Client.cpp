/**
    一个多线程发送文件的服务器
*/
// #include "../filOperate.h"
#include "../filUtil/filOperate.h"
#include "Client.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

//发送文件数据
//buf:数据
//send_size：数据大小
//closeFd : 发送完了是否等待服务端发来的消息，true为发送完立刻退出 否则等待
int Client::sendData(const void* buf ,  int send_size,bool waitServer)
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

        //等待服务器端的消息
        if(waitServer)
        {
            int recv_size = -1;
            char buf[INTSIZE*2];
            memset(buf,0,sizeof(buf));
            for(int i =0 ;i < INTSIZE;i++)
            {
                if(recv(data_send_fd ,buf+i , 1,0 ) == -1)
                {
                    perror("sendData : recv failed\n");
                    exit(-1);
                }
            }
            memcpy(&recv_size , buf , INTSIZE);
            printf("recv_size = %d\n",recv_size);

            int id = -1;

            //接收后面传来的id
            for(int i =0; i< recv_size;i++)
            {
                if(recv(data_send_fd,buf+INTSIZE+i,1,0) == -1)
                {
                    perror("recv(data_send_fd,buf+INTSIZE+i,1,0) failed\n");
                    exit(-1);
                }   
            }

            memcpy(&id , buf+INTSIZE , recv_size);
            send_id = id;
            printf("recv id = %d \n",id);
        }

        delete int_size;
        close(data_send_fd);
        return totalSend;
}

int Client::sendFileInfo(const struct st_filInfo* filInfo)
{
    //发送文件信息
    char* sendBytes = (char*)malloc(sizeof(struct st_filInfo)+INTSIZE);
    printf("sizeof(struct st_filInfo) = %d\n",sizeof(struct st_filInfo));
    
    memset(sendBytes,0,sizeof(sendBytes));
    int send_type = type_filinfo;
    memcpy(sendBytes,(&send_type),INTSIZE);

    memcpy(sendBytes+INTSIZE,filInfo,sizeof(struct st_filInfo));

    sendData(sendBytes,sizeof(struct st_filInfo)+INTSIZE,true);
    free(sendBytes);
}


//发送文件数据
bool Client::sendFile()
{
    
    //先分析要分成个多少个大的数据块进行传输
    int fil_size = filInfo->size;   
    int block_cnt = fil_size / BLOCKSIZE;
    
    //先传输前block_cnt块
    //每一次开一个线程进行传输
    for(int i = 0 ; i < block_cnt ; i++)
    {
        char* block_start = mapMem_p + i*block_cnt;
        
    }



    //如果fil_size / BLOCKSIZE不是整数 还要再传输剩余的一块
    if(fil_size % BLOCKSIZE != 0)
    {

    }

}

//发送一个文件数据块
//需要将文件数据块分成若干更小块进行发送
bool Client::sendBlcok(const char* send_start ,const int send_size,const int offset)
{
    int send_cnt = send_size / SEND_RECV_SIZE;
    int send_type = type_fildata;

    struct st_head* head = (struct st_head*)malloc(sizeof(struct st_head));
    if(head == nullptr)
    {
        perror("head malloc failed\n");
        exit(-1);
    }

    char* send_buf = (char*)malloc(SEND_RECV_SIZE+sizeof(struct st_head)+INTSIZE);
    if(send_buf == nullptr)
    {
        perror("send_buf malloc failed\n");
        exit(-1);
    }
    
    for(int i =0; i < send_cnt;i++)
    {

        //交给一个线程进行传输

        memset(send_buf, 0, SEND_RECV_SIZE+sizeof(struct st_head)+INTSIZE);
        //初始化头部信息
        head->id = send_id;
        head->offset = offset + i*SEND_RECV_SIZE;
        head->size = SEND_RECV_SIZE;

        memcpy(send_buf, &send_type, INTSIZE);
        //拷贝头部
        memcpy(send_buf+INTSIZE, head, sizeof(struct st_head));
        //拷贝数据
        memcpy(send_buf+INTSIZE+sizeof(struct st_head),send_start+i*SEND_RECV_SIZE,SEND_RECV_SIZE);

        struct st_thread_send_arg* arg = (struct st_thread_send_arg*)malloc(sizeof(struct st_thread_send_arg));
        memcpy(arg->send_buf, send_buf,sizeof(struct st_head)+SEND_RECV_SIZE+INTSIZE);
        arg->send_size = SEND_RECV_SIZE+sizeof(struct st_head)+INTSIZE;
        *(arg->clt)  = this;
        //交给线程池发送
        thpool.addTask(threadSend,arg);

        // sendData(send_buf,SEND_RECV_SIZE+sizeof(struct st_head));
    }

    /**
    如果还有剩余 就再发送一次
    */
    if(send_size % SEND_RECV_SIZE !=0)
    {
        int left_send = send_size % SEND_RECV_SIZE;
        char* left_send_buf = (char*)malloc(sizeof(struct st_head)+left_send);
        if(left_send_buf == nullptr)
        {
            perror("left_send_buf malloc failed\n");
            exit(-1);
        }

        memset(left_send_buf, 0, sizeof(struct st_head)+SEND_RECV_SIZE);

        //初始化头部信息
        head->id = send_id;
        head->offset = offset + (send_cnt-1)*SEND_RECV_SIZE;
        head->size = left_send;

        memcpy(left_send_buf, &send_type, INTSIZE);
        //拷贝头部
        memcpy(left_send_buf+INTSIZE, head, sizeof(struct st_head));
        //拷贝数据
        memcpy(left_send_buf+sizeof(struct st_head)+INTSIZE,send_start+(send_cnt-1)*SEND_RECV_SIZE,SEND_RECV_SIZE);

        struct st_thread_send_arg* arg = (struct st_thread_send_arg*)malloc(sizeof(struct st_thread_send_arg));
        memcpy(arg->send_buf, left_send_buf,sizeof(struct st_head)+left_send+INTSIZE);
        arg->send_size = left_send+sizeof(struct st_head)+INTSIZE;
        *(arg->clt)  = this;
        //交给线程池发送
        thpool.addTask(threadSend,arg);
        free(left_send_buf);
    }
    free(head);
    free(send_buf);
    return true;
}

void Client::threadSend(void* arg)
{
    struct  st_thread_send_arg* send_arg = static_cast<struct  st_thread_send_arg*>(arg);
    (*send_arg->clt)->sendData(send_arg->send_buf, send_arg->send_size);
    free(send_arg->clt);
    free(arg);
}