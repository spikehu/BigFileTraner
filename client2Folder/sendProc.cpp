#include "client.h"

int main(int argc ,char* argv[])
{
    if(argc!=4)
    {
        printf("Using:./sendProc  [ip] [port] [filname]\n");
        return -1;
    }
    Client client(argv[1],argv[2],argv[3]);

    //开始发送文件内容
    client.sendFile();
    

    return 0;
}