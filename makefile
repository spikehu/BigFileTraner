all:test

test:test.cpp
	g++ -o test test.cpp -I./ ./ThreadPool.cpp -lpthread -std=c++11

clean:
	rm test -f