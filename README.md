# chat
C++ boost::asio implementation of multi-threaded chat server, and client

I implemented a console mode chat room using C++11 features and boost::asio asynchronous library. The chat room consists of a chat server and multiple chat clients. The makefile will generates 2 binaries, chat_server and chat_client.

The chat room can perform the following functions:
1.	Broadcast each new chat message to all participants in the same room.
2.	A chat message consists of server time stamp, client’s nickname, and client’s chat content text message.
3.	When a new participant joins a room, all recent chat history will be feed to this participant.
4.	A single server can support multiple chat rooms. Chat rooms are distinguished from each other by port numbers.
5.	The server can be easily configured with multiple threads. Number of threads is hardcoded in the server code just for demonstration purpose. For production use, the number of worker threads need a config file entry.
6.	For Linux system, set cpu affinity to threads in pool is also demonstrated.
7.	Tested across Windows and Linux.
