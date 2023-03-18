#include  "EpollTcp.h"


int main(int argc , char* argv[])
{
   EpollTcp server(argv[1]);
   server.wait();

    return 0;
}