#include "filOperate.h"

int main(int argc ,char* argv[])
{

    //初始化文件信息
    struct st_filInfo* st_fil = initFileInfo("test.cpp");
    char* p = myMmap(st_fil);
    printf("%s",p);
    return 0;
}