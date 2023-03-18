all: server 

server:server.cpp
	g++ -o server server.cpp  -g -I./  ThreadPool.cpp  EpollTcp.cpp -lpthread    -std=c++11 	

clean:
	server