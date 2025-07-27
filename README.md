# EX3 – Event-Driven Chat Server in C

Simple multi-client chat server implemented in C using non-blocking sockets and the `select()` system call.

---

## 👩‍💻 Author

**Raghad Alyan**  
Azrieli College of Engineering

---

## 📁 Files

- `chatServer.c` – Core implementation of the server

---

## 🧠 Overview

This project builds a **basic multi-client chat server** in C using:

- `select()` for handling multiple sockets without threads  
- Non-blocking I/O via `ioctl()`  
- A connection pool for managing clients  
- A per-client message queue  
- Graceful shutdown using `SIGINT` (Ctrl+C)  

---

## 🧱 Architecture

### ✅ Signal Handling

- `SIGINT` (Ctrl+C) is caught by a custom handler.
- The server uses a flag (`end_server`) to exit the main loop safely.

### ✅ Main Responsibilities

- Validate input arguments (port number)
- Initialize and manage the connection pool
- Create and bind a non-blocking listening socket
- Accept client connections and read/write messages using `select()`
- Queue messages and broadcast them to other clients
- Clean up memory and sockets when the server shuts down

### ✅ Core Functions

#### Connection Pool Functions:
- `initPool()` – Initialize connection pool and FD sets  
- `addConn()` – Add new client socket to pool  
- `removeConn()` – Remove client and free resources  
- `updateMaxFd()` – Track the highest file descriptor  
- `removeFromPool()` – Remove connection from list and free memory  
- `cleanupConnection()` – Close sockets and update FD sets  

#### Message Queue Functions:
- `addMsg()` – Add a message to all clients’ queues (except sender)  
- `writeToClient()` – Send queued messages to clients  

---

## 🧪 How to Compile

Basic compilation:

gcc -Wall -o chatServer chatServer.c

With Valgrind (for memory checking):
valgrind --leak-check=full ./chatServer <port>
---

## 💬 How to Run

Start the server:
./chatServer <port>

Example:
./chatServer 5555

Open another terminal and connect with:
telnet localhost 5555

Repeat in multiple terminals to simulate multi-client chat.

---

## 📌 Example Output
Waiting on select()... MaxFd 5
New incoming connection on sd 4
Descriptor 4 is readable
32 bytes received from sd 4
Connection closed for sd 4
removing connection with sd 4
