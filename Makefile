all: server

server:
	g++ server.cpp -o 550server -std=c++11
client:
	g++ client.c -o build/client.out
epoll-server:
	gcc epoll-example.c -o build/epoll-server.out
