# EX3 – Event-Driven Chat Server in C

Simple multi-client chat server implemented in C using non-blocking sockets and the `select()` system call.

---

## 👩‍💻 Author
**Raghad Alyan**  
Student ID: 324231604  
Azrieli College of Engineering

---

## 📁 Files

- `chatServer.c` – Core implementation (single C file)

---

## 🧠 Overview

This project builds a **basic chat server** using:

- `select()` for I/O multiplexing (no threads)
- File descriptor sets to track client states
- Non-blocking sockets with `ioctl()`
- Per-connection message queues
- Graceful exit using `SIGINT` handling

---

## 🧱 Architecture

### ✅ **Signal Handling**
- Listens for `SIGINT` (Ctrl+C)
- Sets a flag to gracefully shut down the server

### ✅ **Main Function Responsibilities**
- Parse and validate command-line port
- Initialize connection pool
- Set up non-blocking listening socket
- Accept and manage client connections
- Use `select()` loop to:
  - Accept new clients
  - Read from clients
  - Write queued messages
- Clean up memory and close sockets on shutdown

### ✅ **Key Functions**
#### Connection Pool:
- `initPool()` – Initialize pool data
- `addConn()` / `removeConn()` – Manage client sockets
- `updateMaxFd()` – Track max file descriptor
- `removeFromPool()` – Free connection memory
- `cleanupConnection()` – Close socket, clear sets

#### Messaging:
- `addMsg()` – Queue messages for clients
- `writeToClient()` – Send messages from queue

---

## 🚀 How to Compile

```bash
gcc -Wall -o chatServer chatServer.c
