# Client-Server File Request System

## Introduction

This project implements a client-server architecture where clients can request files or sets of files from a server. The server searches its file directory rooted at `~` and returns the requested files to the client or an appropriate message if the files are not found. Multiple clients can connect to the server from different machines and make file requests as specified in the commands below.

The server, along with two mirrors (`mirror1` and `mirror2`), and the client processes run on separate machines/terminals and communicate exclusively using sockets.

## Server (`serverw24`)

### Overview
- `serverw24` and two identical copies (`mirror1` and `mirror2`) must be running before any clients connect.
- The server listens for connection requests and forks a child process to handle each client request through the `crequest()` function.
- The `crequest()` function handles client commands in an infinite loop and terminates upon receiving the `quitc` command.

### Handling Requests
- **dirlist -a**: Returns a list of subdirectories under the server's home directory in alphabetical order.
  - Example: `clientw24$ dirlist -a`
- **dirlist -t**: Returns a list of subdirectories under the server's home directory by creation date (oldest first).
  - Example: `clientw24$ dirlist -t`
- **w24fn filename**: Returns the filename, size, date created, and file permissions if found in the server's directory tree.
  - Example: `client24$ w24fn sample.txt`
- **w24fz size1 size2**: Returns a `temp.tar.gz` archive of files within the specified size range.
  - Example: `client24$ w24fz 1240 12450`
- **w24ft <extension list>**: Returns a `temp.tar.gz` archive of files with the specified extensions (up to 3 different types).
  - Example: `client24$ w24ft c txt`
  - Example: `client24$ w24ft jpg bmp pdf`
- **w24fdb date**: Returns a `temp.tar.gz` archive of files created on or before the specified date.
  - Example: `client24$ w24fdb 2023-01-01`
- **w24fda date**: Returns a `temp.tar.gz` archive of files created on or after the specified date.
  - Example: `client24$ w24fda 2023-03-31`
- **quitc**: Terminates the client process.

## Client (`clientw24`)

### Overview
- The client runs in an infinite loop, waiting for user input.
- Commands are verified for syntax correctness before being sent to the server.
- Appropriate error messages are displayed for invalid commands.

### Commands
- **dirlist -a**: Request subdirectory list in alphabetical order.
- **dirlist -t**: Request subdirectory list by creation date.
- **w24fn filename**: Request file information.
- **w24fz size1 size2**: Request files within a size range.
- **w24ft <extension list>**: Request files with specified extensions.
- **w24fdb date**: Request files created on or before a date.
- **w24fda date**: Request files created on or after a date.
- **quitc**: Terminate the client process.

### Notes
- All files returned from the server are stored in a `w24project` folder in the client's home directory.
- The client verifies command syntax and prints appropriate error messages for invalid commands.

## Server Load Balancing

### Alternating Between `serverw24`, `mirror1`, and `mirror2`
- The first 3 client connections are handled by `serverw24`.
- The next 3 connections (4-6) are handled by `mirror1`.
- The next 3 connections (7-9) are handled by `mirror2`.
- Subsequent connections are handled in a round-robin fashion:
  - Connection 10: `serverw24`
  - Connection 11: `mirror1`
  - Connection 12: `mirror2`
  - And so on...

## Conclusion

This project demonstrates a robust client-server architecture with load balancing and command-specific file handling. Clients can interact with the server to request and retrieve files based on a variety of criteria, ensuring efficient file management and retrieval.

## Getting Started

To run the project, start `serverw24`, `mirror1`, and `mirror2` on separate machines/terminals, and then start the `clientw24` process on a different machine/terminal.

### Prerequisites
- Ensure all machines/terminals are networked and can communicate via sockets.
- Place the server and client scripts in the respective home directories.

### Running the Server and Clients
1. Start `mirro1`:
   ```sh
   ./mirror1 

2. Start `mirro2`:
   ```sh
   ./mirror2

3. Start `server`:
   ```sh
   ./server <server-port>

4. Start `client`:
   ```sh
   ./client <server-ip> <server-port>

