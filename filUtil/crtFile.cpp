#include "filOperate.h"
#include <cstdio>
#include <cstdlib>


//创建指定大小的文件
//使用: ./crtFile fileName Size
//向里面传入数据
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
    char* p = myMmap(argv[1],atoi(argv[2]));

    //写入内容 写 10万行hello world
    for(int i = 0 ;i < 8730;i++)
    {
        memcpy(p+i*sizeof("hello wolrd\n"),"hello wolrd\n",sizeof("hello wolrd\n"));
    }

    return 0;
}