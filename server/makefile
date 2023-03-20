all: server 

server:server.cpp
	g++ -o server server.cpp  -g -I./  -I../filUtil ../filUtil/filOperate.cpp ThreadPool.cpp  EpollTcp.cpp -lpthread    -std=c++11 	

clean:
	rm -f server