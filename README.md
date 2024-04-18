Introduction
This project is a client-server system where a client can request files or sets of files from the server. The server searches for the requested files in its directory tree and returns them to the client. Multiple clients can connect to the server from different machines and request files using specific commands.

Section 1: Server (serverw24)
The serverw24 and two identical copies, mirror1 and mirror2, run on different machines/terminals.
Upon receiving a connection request, serverw24 forks a child process to handle the client's request exclusively.
The child process executes a function called crequest(), which waits for commands from the client, processes them, and returns the results.
The serverw24, mirror1, and mirror2 must communicate using sockets only.

Section 2: Client (clientw24)
The client process waits for the user to enter commands, which are sent to the serverw24.
Commands are not Linux commands but specific actions defined for this project.
The client verifies the syntax of the command before sending it to the serverw24.
List of Client Commands:
dirlist -a: Returns subdirectories/folders under the server's home directory in alphabetical order.
dirlist -t: Returns subdirectories/folders under the server's home directory in the order of creation.
w24fn filename: Returns information about a file, including filename, size, date created, and permissions.
w24fz size1 size2: Returns files within a specified size range.
w24ft <extension list>: Returns files with specific file types.
w24fdb date: Returns files created on or before a specified date.
w24fda date: Returns files created on or after a specified date.
quitc: Terminates the client process.

Section 3: Alternating Between serverw24, mirror1, and mirror2
The serverw24, mirror1, and mirror2 run on three different machines/terminals.
The first 3 client connections are handled by serverw24.
Client connections 4-6 are handled by mirror1.
Client connections 7-9 are handled by mirror2.
Subsequent client connections are handled by serverw24, mirror1, and mirror2 in an alternating manner. For example, connection 10 is handled by serverw24, connection 11 by mirror1, connection 12 by mirror2, and so on.
