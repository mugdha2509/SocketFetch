#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
 
#define SERVER_IP "127.0.0.1" // localhost
#define PORT 8888
#define MAXDATASIZE 1024
 #define BUFFER_SIZE 1024

 
// Function to send commands to the server and receive responses
void sendRequest(int client_socket, const char *command) {
    char buffer[MAXDATASIZE];
    int num_bytes_recv;
 
    // Send command to server
    printf("Sending command to server: %s\n", command); // Debug statement
    if (send(client_socket, command, strlen(command), 0) == -1) {
        perror("Send failed");
        return;
    }
 
    if (strcmp(command, "quitc") == 0) {
        return; // No need to receive response for quit command
    }
 
    // Receive response from server
    num_bytes_recv = recv(client_socket, buffer, MAXDATASIZE, 0);
    if (num_bytes_recv == -1) {
        printf("Receive failed");
        return;
    } else if (num_bytes_recv == 0) {
        printf("Server closed the connection\n");
        return;
    } else {
        buffer[num_bytes_recv] = '\0';
        printf("Received %d bytes from server: %s\n", num_bytes_recv, buffer); // Debug statement
 
        // Print received data
        printf("Received data from server: %s\n", buffer);
    }
}

// Function to establish connection to the server
int makeConnection() {
    int client_socket;
    struct sockaddr_in server_addr;

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return -1;
    }

    // Server address setup
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    memset(&(server_addr.sin_zero), '\0', 8);

    // Print IP address and port where the code is being sent
    printf("Sending code to %s:%d\n", SERVER_IP, PORT);

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("Connection failed");
        close(client_socket);
        return -1;
    }

    return client_socket; // Return the client socket descriptor
}

 // Function to receive and save the tar file from the server
void getTarfile(int client_socket, long tar_size) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    long total_received = 0;

    // Open file for writing
    FILE *tar_file = fopen("temp.tar.gz", "wb");
    if (tar_file == NULL) {
        perror("Error opening tar file for writing");
        return;
    }

    // Receive tar file data from server and write it to the file
    while (total_received < tar_size) {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            perror("Error receiving tar file data from server");
            break;
        } else if (bytes_received == 0) {
            printf("Server closed connection.\n");
            break;
        }
        fwrite(buffer, 1, bytes_received, tar_file);
        total_received += bytes_received;
    }

    // Close the file
    fclose(tar_file);
}

void getandCreateTar(int client_socket) {
    char buffer[BUFFER_SIZE];

    // Open temporary file to store file paths
    FILE *temp_file = fopen("temp_file_paths.txt", "w");
    if (!temp_file) {
        perror("Error creating temporary file");
        return;
    }

    // Receive and save file paths from server
    ssize_t bytes_received;
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, temp_file);
    }

    if (bytes_received < 0) {
        perror("Error receiving file paths from server");
        fclose(temp_file);
        return;
    }

    fclose(temp_file);

    // Check if the tar file already exists
    if (access("temp.tar.gz", F_OK) == 0) {
        // If it exists, remove it
        if (remove("temp.tar.gz") != 0) {
            perror("Error removing existing tar file");
            send(client_socket, "Error removing existing tar file", strlen("Error removing existing tar file"), 0);
            return;
        }
    }

    // Create tar file using the received file paths
    int ret = system("tar -czf temp.tar.gz -T temp_file_paths.txt");
    if (ret == -1) {
        perror("Error creating tar file");
        send(client_socket, "Error creating tar file", strlen("Error creating tar file"), 0);
    } else {
        // Notify user about successful tar file creation
        printf("Tar file created successfully.\n");
        printf("Sending success message to server...\n");
        send(client_socket, "Tar file created successfully", strlen("Tar file created successfully"), 0);
        printf("Success message sent to server.\n");
    }
}
 
int main() {
    int client_socket;
    char command[MAXDATASIZE];
 
    // Establish connection to the server
    client_socket = makeConnection();
    if (client_socket == -1) {
        fprintf(stderr, "Failed to establish connection to the server\n");
        exit(EXIT_FAILURE);
    }
 
    // Inside main function
    while (1) {
        printf("Enter command: ");
        fgets(command, MAXDATASIZE, stdin);
        command[strcspn(command, "\n")] = '\0';
 
    // Validate command syntax
    if (strcmp(command, "dirlist -a") != 0 && strcmp(command, "dirlist -t") != 0 &&
    strcmp(command, "quitc") != 0 && strncmp(command, "w24fn ", 6) != 0 &&
    strncmp(command, "w24fz", 5) != 0  && strncmp(command, "w24fdb", 6) != 0 &&
    strncmp(command, "w24fda", 6) != 0 && strncmp(command, "w24ft", 5) != 0) {
    printf("Invalid command. Please enter a valid command\n");
    continue;
    }


 
        // Send command to server and receive response
        sendRequest(client_socket, command);
 
        // Check if quit command is entered
        if (strcmp(command, "quitc") == 0) {
            break;
        }

        //   // Check if the command is "w24ft" and handle creating a tar file
        // if (strncmp(command, "w24ft", 5) == 0) {
        //     // Receive file paths from server and create a tar file
        //     getandCreateTar(client_socket);
        // }
    }
 
    // Close socket
    close(client_socket);
    printf("Client socket closed\n"); // Debug statement
 
    return 0;
}

