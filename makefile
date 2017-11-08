CC      = g++
CFLAGS  = -O3
OPTION  = -std=c++14
LIBS    = -lboost_system -lboost_thread -pthread

all: chat_server chat_client

chat_server: chat_server.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

chat_client: chat_client.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
	
chat_server.o: chat_server.cpp protocol.hpp
	$(CC) $(OPTION) -c $(CFLAGS) chat_server.cpp

chat_client.o: chat_client.cpp protocol.hpp
	$(CC) $(OPTION) -c $(CFLAGS) chat_client.cpp

.PHONY: clean

clean:
	rm *.o
	rm chat_server chat_client
