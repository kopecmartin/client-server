# Description: Makefile for client.cpp and server.cpp
# Author: Martin Kopec
# Login: xkopec42
# Date: 23.04.2016
##################

NAME=all

CC=g++
CFLAGS=-std=c++11 -pthread -static-libstdc++ -Wextra -Wall -pedantic 

all: server client

client: client.cpp
	$(CC) $(CFLAGS) client.cpp -o client

server: server.cpp
	$(CC) $(CFLAGS) server.cpp -o server
	
clean:
	rm -f server
	rm -f client
