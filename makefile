all: server client

server: socket_server.cpp
	g++ socket_server.cpp -o server -pthread -lncurses
client: socket_client.cpp
	g++ socket_client.cpp -o client -pthread -lncurses 
clean:
	rm -rf *.o *~ server client