Simple chat server implemented in C
Authored by RAGHAD ALYAN
324231604 
============================================================================================================
CLASSES :
chatServer.c 
===========================================================================================================
Header File Inclusion: The code includes the necessary header files for socket programming, signal handling, memory allocation, and file descriptor manipulation.

Signal Handling: A signal handler (intHandler) is defined to catch the SIGINT signal (Ctrl+C), which sets the end_server flag to 1, indicating that the server should terminate gracefully.

Main Function:

It checks the command-line arguments to ensure that the correct number of arguments (port number) is provided.
Initializes the connection pool (conn_pool_t) and sets up the listening socket (listen_sd) for incoming connections.
Sets the listening socket to non-blocking mode and binds it to the specified port.
Starts listening for incoming connections and adds the listening socket to the connection pool.
Initializes file descriptor sets (read_set, write_set, ready_read_set, ready_write_set) and adds active connections to them.
Enters a loop where it monitors file descriptors for read and write events using the select function.
Handles incoming connections, reads data from clients, and writes data to clients based on the readiness of file descriptors.
Gracefully cleans up resources (closing sockets, freeing memory) upon server termination.
Connection Pool Functions:

initPool: Initializes the connection pool.
addConn: Adds a new connection to the connection pool.
removeConn: Removes a connection from the connection pool.
updateMaxFd: Updates the maximum file descriptor value in the connection pool.
removeFromPool: Removes a connection from the connection pool and frees associated memory.
freeMessageQueue: Frees memory allocated for the message queue of a connection.
cleanupConnection: Cleans up resources associated with a connection.
Message Queue Functions:

addMsg: Adds a message to the write queue of connections in the connection pool.
writeToClient: Writes messages from the write queue to clients.
Overall, the code provides a basic framework for a chat server capable of handling multiple concurrent connections using non-blocking I/O and the select function for event-driven programming.
=================================================================================================================
==How to compile?==
gcc -o chatServer chatServer.c
=================================================================================================================
==How to pass arguments?== 
./chatServer <port>
=================================================================================================================
==VALGRIND?==
gcc -g -o chatServer chatServer.c
valgrind --leak-check=full ./chatServer
=================================================================================================================
RAGHAD ALYAN 