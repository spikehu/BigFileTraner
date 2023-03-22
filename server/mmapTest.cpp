#include "../filUtil/filOperate.h"
#include <cstring>

int main(int argc , char* argv[])
{
    char* p = myMmap("./upload/trans",18);
    while(1)
    {
        memcpy(p, "hello!~!!!!",sizeof("hello!~!!!!"));
    }
    return 0;
}