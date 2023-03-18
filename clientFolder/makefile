all:sendProc

sendProc:sendProc.cpp
	g++ -g -o sendProc sendProc.cpp -I../filUtil ../filUtil/filOperate.cpp -I./ Client.cpp ThreadPool.cpp -lpthread -std=c++11

clean:
	rm -f sendProc 