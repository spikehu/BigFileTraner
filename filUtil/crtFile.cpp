#include "filOperate.h"
#include <cstdio>
#include <cstdlib>


//创建指定大小的文件
//使用: ./crtFile fileName Size
int main(int argc ,char* argv[])
{
    if(argc!=3)
    {
        printf("Using: ./crtFile [filname] [size]\n");
        return -1;
    }
    if(creat_fil(argv[1],atoi(argv[2]))==false)
    {
        printf("creat file failed\n");
        return -1;
    }
    return 0;
}