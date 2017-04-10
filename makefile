all: server

server: command.cpp command.h database_connection.cpp database_connection.h server.cpp
	$(CXX) *.cpp -lsfml-network -lsfml-system -o server -std=c++11 -pthread -ggdb -lmysqlcppconn

clean:
	rm server
