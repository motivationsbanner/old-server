all: server.cpp
	clear # clear consle to only view the new compiler messages
	printf '\033[3J' # clear putty buffer
	g++ server.cpp -lsfml-network -lsfml-system -o server -std=c++14 -pthread -ggdb
