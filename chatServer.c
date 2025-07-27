#include "chatServer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>


void updateMaxFd(conn_pool_t* pool, conn_t* next_conn);
void removeFromPool(conn_pool_t* pool, conn_t* conn);
void freeMessageQueue(msg_t* msg);
void cleanupConnection(int sd, conn_pool_t* pool);

static volatile int end_server = 0;

void intHandler(int SIG_INT) {
    end_server = 1;
}

int main(int argc, char *argv[]) {
// Check if the correct number of arguments is provided
    if (argc != 2) {
        printf("Usage: server <port>");
        exit(EXIT_FAILURE);
    }
    // Convert the command-line argument to an integer
    int port = atoi(argv[1]);

    // Define the maximum port number allowed (65535)
    int max_port = 65535;

    if (port <0 || port > max_port){
        printf("Usage: server <port>");
        exit(EXIT_FAILURE);
    }

//  SIGINT (Ctrl+C)
    signal(SIGINT, intHandler);

// Allocate memory for the connection pool and init it
    conn_pool_t* pool = malloc(sizeof(conn_pool_t));
    if (initPool(pool) == -1) {
        perror("Failed to initialize pool");
        return 1;
    }

// Create a TCP socket
    int listen_sd;
    if ((listen_sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return 1;
    }

// Set the socket to non-blocking mode
    int on = 1;
    if (ioctl(listen_sd, FIONBIO, (char *)&on) == -1) {
        perror("ioctl");
        return 1;
    }

// Set up the address structure
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));

// Bind the socket
    if (bind(listen_sd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

// Start listening on the socket
    if (listen(listen_sd, 10) == -1) {
        perror("listen");
        return 1;
    }

// Initialize file descriptor sets
    FD_ZERO(&pool->write_set);
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->ready_read_set);
    FD_ZERO(&pool->ready_write_set);

// Add the listening socket descriptor to the connection pool
    if (addConn(listen_sd, pool) == -1) {
        perror("Failed to add listening socket descriptor to the pool");
        close(listen_sd);
        free(pool);
        exit(EXIT_FAILURE);
    }

// Add active connections to sets
    if (pool->conn_head == NULL) {
        perror("Error: Connection pool head is NULL\n");
        exit(0);
    }

// Traverse the connection list to set file descriptors in the read and write sets
    conn_t *curr_conn = pool->conn_head;
    while (curr_conn != NULL) {
        // Set the file descriptor for reading
        FD_SET(curr_conn->fd, &pool->read_set);

        // If there are pending messages, set the file descriptor for writing
        if (curr_conn->write_msg_head != NULL) {
            FD_SET(curr_conn->fd, &pool->write_set);
        }

        // Update the maximum file descriptor value
        if (curr_conn->fd > pool->maxfd) {
            pool->maxfd = curr_conn->fd;
        }

        // Move to the next connection in the list
        curr_conn = curr_conn->next;
    }

// Update the maximum file descriptor to the listening socket descriptor
    pool->maxfd = listen_sd;

//************************************************************
    // Server
    while (end_server == 0) {

        // Make a copy of the sets for select
        memcpy(&pool->ready_read_set, &pool->read_set, sizeof(fd_set));
        memcpy(&pool->ready_write_set, &pool->write_set, sizeof(fd_set));

        printf("Waiting on select()...\nMaxFd %d\n", pool->maxfd);

        int count = 0;

        // select
        pool->nready = select(pool->maxfd + 1, &pool->ready_read_set, &pool->ready_write_set, NULL, NULL);
        if (pool->nready < 0) {
            perror("Error in select");
            continue;
        }

        // Handle listening socket
        if (FD_ISSET(listen_sd, &pool->ready_read_set)) {
            // Accept new connection
            int new_sd = accept(listen_sd, NULL, NULL);
            if (new_sd < 0) {
                perror("Error accepting new connection");
            } else {
                printf("New incoming connection on sd %d\n", new_sd);
                // Add new connection to the pool
                if (addConn(new_sd, pool) == -1) {
                    perror("Error adding new connection");
                }
            }
        }

        // Handle active connections
        curr_conn = pool->conn_head;
        while (curr_conn != NULL) {

            if (count == pool->nready) {
                break;
            }

            conn_t* next_conn = curr_conn->next;
            int sd = curr_conn->fd;
            if (sd == listen_sd) {
                curr_conn = curr_conn->next;
                continue;
            }
            if (FD_ISSET(sd, &pool->ready_read_set)) {
                // Read from client
                printf("Descriptor %d is readable\n", sd);

                count++;
                char buffer[BUFFER_SIZE];
                int len = read(sd, buffer, BUFFER_SIZE);
                if (len < 0) {
                    perror("Error reading from client");
                } else if (len == 0) {
                    // Connection closed by client
                    removeConn(sd, pool);
                    printf("Connection closed for sd %d\n",sd);
                } else {
                    // Message received from client
                    printf("%d bytes received from sd %d\n", len, sd);
                    if (addMsg(sd, buffer, len, pool) == -1) {
                        perror("Failed to add message");
                    }
                }
            }
            if (FD_ISSET(sd, &pool->ready_write_set)) {
                // Write to client
                count++;
                if (writeToClient(sd, pool) == -1) {
                    perror("Error writing to client");
                }
            }
            curr_conn = next_conn;
        }
    }

//************************************************************

    // Close file descriptors and free memory
    conn_t *curr = pool->conn_head->next;
    while (curr != NULL) {
        close(curr->fd);
        curr = curr->next;
    }
    free(pool);


    return 0;
}

////------------------------------------------------------------------------------------------------------------------------------------------------------------------
////------------------------------------------------------------------------------------------------------------------------------------------------------------------
//// FUNCTIONS
//************************************************************
// Function to initialize the pool
int initPool(conn_pool_t* pool) {
    if (!pool) return -1;
    pool->maxfd = 0;
    pool->nready = 0;
    pool->nr_conns = 0;
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->ready_read_set);
    FD_ZERO(&pool->write_set);
    FD_ZERO(&pool->ready_write_set);
    pool->conn_head = NULL;

    return 0;
}

//************************************************************
// Function to add a new connection to the connection pool
int addConn(int sd, conn_pool_t* pool) {
    // Check if the connection pool pointer is valid
    if (!pool)
        return -1;

    // Allocate memory for the new connection
    conn_t *new_conn = (conn_t *)malloc(sizeof(conn_t));
    // Check if memory allocation was successful
    if (!new_conn)
        return -1;

    // Initialize the new connection with the provided socket descriptor
    new_conn->fd = sd;
    new_conn->write_msg_head = NULL;
    new_conn->write_msg_tail = NULL;

    // If the connection pool is empty, set the new connection as the head
    if (pool->conn_head == NULL) {
        new_conn->prev = new_conn->next = NULL;
        pool->conn_head = new_conn;
    }
        // Otherwise, insert the new connection at the beginning of the list
    else {
        new_conn->prev = NULL;
        new_conn->next = pool->conn_head;
        pool->conn_head->prev = new_conn;
        pool->conn_head = new_conn;
    }

    // Increment the number of connections in the pool
    pool->nr_conns++;
    // Update the maximum file descriptor if necessary
    if (sd > pool->maxfd) pool->maxfd = sd;
    // Add the socket descriptor to the read set
    FD_SET(sd, &(pool->read_set));
    // Return success
    return 0;
}

//************************************************************
// Function to remove a connection from the connection pool
int removeConn(int sd, conn_pool_t* pool) {
    if (pool == NULL) {
        return -1; // Invalid pool pointer
    }

    // Find the connection to remove
    conn_t *curr_conn = pool->conn_head;
    while (curr_conn != NULL) {
        if (curr_conn->fd == sd) {
            // Update the maximum file descriptor
            updateMaxFd(pool, curr_conn->next);

            // Remove the connection from the pool
            removeFromPool(pool, curr_conn);

            // Close the socket and clear file descriptor sets
            cleanupConnection(sd, pool);

            printf("Connection with sd %d removed\n", sd);
            return 0; // Connection removed successfully
        }
        curr_conn = curr_conn->next;
    }

    return -1;
}

// Function to update the maximum file descriptor value in the connection pool
void updateMaxFd(conn_pool_t* pool, conn_t* next_conn) {
    if (next_conn == NULL) {
        pool->maxfd = 0; // No more connections in the pool
    } else if (pool->maxfd == next_conn->fd) {
        pool->maxfd = next_conn->fd;
    }
}

// Function to remove a connection from the connection pool
void removeFromPool(conn_pool_t* pool, conn_t* conn) {
    if (conn->prev != NULL) {
        conn->prev->next = conn->next;
    } else {
        pool->conn_head = conn->next;
    }

    if (conn->next != NULL) {
        conn->next->prev = conn->prev;
    }

    // Free message queue if present
    freeMessageQueue(conn->write_msg_head);

    free(conn);

    pool->nr_conns--;
}

//function to free the memory allocated for the message queue
void freeMessageQueue(msg_t* msg) {
    while (msg != NULL) {
        msg_t* temp = msg;
        msg = msg->next;
        free(temp->message);
        free(temp);
    }
}

//function to clean up resources associated with a connection
void cleanupConnection(int sd, conn_pool_t* pool) {
    close(sd);
    FD_CLR(sd, &(pool->read_set));
    FD_CLR(sd, &(pool->write_set));
}


//************************************************************
// Function to add a message to the write queue of connections in the connection pool
int addMsg(int sd, char* buffer, int len, conn_pool_t* pool) {
    // Check if the connection pool pointer is valid and input buffer is not empty
    if (pool == NULL || buffer == NULL || len <= 0) {
        return -1; // if pool is invalid or input buffer is empty
    }

    // Traverse the connection list to add the message to each connection's write queue
    conn_t *curr_conn = pool->conn_head;
    while (curr_conn != NULL && curr_conn->next != NULL) {
        // Skip if the current connection is the sender
        if (curr_conn->fd != sd) {
            // Allocate memory for the new message
            msg_t *new_msg = (msg_t*)malloc(sizeof(msg_t));
            if (new_msg == NULL) {
                return -1;
            }

            // Allocate memory for the message content and copy the buffer data
            new_msg->message = (char *)malloc(len + 1);
            if (new_msg->message == NULL) {
                free(new_msg);
                return -1;
            }
            strncpy(new_msg->message, buffer, len);
            new_msg->message[len] = '\0';
            new_msg->size = len;

            // Initialize pointers for the new message
            new_msg->prev = new_msg->next = NULL;

            //add the message to the connection's write queue
            if (curr_conn->write_msg_head == NULL) {
                curr_conn->write_msg_head = new_msg;
                curr_conn->write_msg_tail = new_msg;
            } else {
                curr_conn->write_msg_tail->next = new_msg;
                new_msg->prev = curr_conn->write_msg_tail;
                curr_conn->write_msg_tail = new_msg;
            }

            // Update the file descriptor set to indicate that this connection is ready to write
            FD_SET(curr_conn->fd, &pool->write_set);
        }

        // Move to the next connection in the list
        curr_conn = curr_conn->next;
    }

    return 0;
}


//************************************************************
// Function to write messages from the write queue to clients
int writeToClient(int sd, conn_pool_t* pool) {
    // Check if the connection pool pointer is valid
    if (pool == NULL) {
        return -1;
    }

    // Traverse the connection list to write messages to clients
    conn_t *curr_conn = pool->conn_head;
    while (curr_conn != NULL) {
        // Check if the current connection is ready for writing and has pending messages
        if (FD_ISSET(curr_conn->fd, &pool->write_set) && curr_conn->write_msg_head != NULL) {
            msg_t *msg = curr_conn->write_msg_head; // Get the first message in the write queue
            int bytes_written = 0;

            // Loop until the entire message is written to the client
            while (bytes_written < msg->size) {
                // Write the message to the client socket
                int ret = write(curr_conn->fd, msg->message + bytes_written, msg->size - bytes_written);
                if (ret < 0) {
                    perror("send failed\n");
                    return -1;
                } else if (ret == 0) {

                    return -1; // Return -1 in case of unexpected write return value
                } else {
                    bytes_written += ret;
                }
            }

            // Remove the sent message from the write queue
            curr_conn->write_msg_head = msg->next;
            if (curr_conn->write_msg_head == NULL) {
                curr_conn->write_msg_tail = NULL;
            }

            // Free memory allocated
            free(msg->message);
            free(msg);
        }

        // Clear the write flag for this client
        FD_CLR(curr_conn->fd, &pool->write_set);

        // Move to the next connection in the list
        curr_conn = curr_conn->next;
    }

    return 0;
}


