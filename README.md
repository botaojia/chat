# chat
C++ boost::asio implementation of multi-threaded chat server, and client

I implemented a console mode chat room using C++11 features and boost::asio asynchronous library. The chat room consists of a chat server and multiple chat clients. The makefile will generate 2 binaries, chat_server and chat_client. The boost::asio::io_service::strand is used to serialize server side event handlers in thread pool. The strand wrapper eliminates the need of explicitly synchronize and locking event handlers in thread pool.

The chat room can perform the following functions:
1.	Broadcast each new chat message to all participants in the same room.
2.	A chat message consists of server time stamp, client’s nickname, and client’s chat content text message.
3.	When a new participant joins a room, all recent chat history will be feed to this participant.
4.	A single server can support multiple chat rooms. Chat rooms are distinguished from each other by port numbers.
5.	The server can be easily configured with multiple threads. Number of threads is hardcoded in the server code just for demonstration purpose. For production use, the number of worker threads need a config file entry.
6.	For Linux system, set cpu affinity to threads in pool is also demonstrated.
7.	Tested across Windows and Linux.

# example
starts server on a home computer, with only one room listening on port 8888
>$./chat_server 8888

starts 3 clients on another home computer, all connect to the chat_server which is on box 192.168.1.4.

first starts client Botao:
>$./chat_client Botao 192.168.1.4 8888

After a few typing messages, starts client Tom:
>$./chat_client Tom 192.168.1.4 8888

Finally starts client Mike:
>$./chat_client Mike 192.168.1.4 8888

The type in message and console history is like this:
![image](https://github.com/botaojia/chat/blob/master/example.png)

The chat_server can support multiple room like this:
>$./chat_server 8888 9999

now clients can select which room to join based on port numbers.

