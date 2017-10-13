all: server client

server:
	g++ -g server.cpp -o 550server -std=c++14 -lpthread
client:
	g++ client.c -o build/client.out
