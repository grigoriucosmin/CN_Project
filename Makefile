all: client server

client: server
	g++ -pthread client.cpp -o client.sh

server:
	g++ -pthread server.cpp -l sqlite3 -o server.sh
