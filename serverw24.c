#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>

#define PORT 8888
#define BACKLOG 15
#define MAXDATASIZE 1024
#define MIRROR1_IP "127.0.0.1"
#define MIRROR1_PORT 8889
#define MIRROR2_IP "127.0.0.1"
#define MIRROR2_PORT 8890
#define PATH_MAX_LENGTH 512 
#define TAR_COMMAND_FMT "tar -czf %s/temp.tar.gz -C %s %s"
#define MAX_EXTENSIONS 3
#define PERMISSIONS 0777
#define BUFFER_SIZE 1024
#define MAX_DIRS 100


// Declare tar_fd as a global variable
int tar_fd;

// Structure to hold directory name and its creation time
typedef struct {
    char name[256];
    time_t creation_time;
} DirInfo;

// Comparator function for sorting directories by creation time
int creationTimecompare(const void *a, const void *b) {
    const DirInfo *dir_info_a = (const DirInfo *)a;
    const DirInfo *dir_info_b = (const DirInfo *)b;
    return difftime(dir_info_a->creation_time, dir_info_b->creation_time);
}

// Comparator function for sorting directory names alphabetically
int dirCompare(const void *a, const void *b) {
    const char *dir_name_a = *(const char **)a;
    const char *dir_name_b = *(const char **)b;
    return strcasecmp(dir_name_a, dir_name_b);
}

// compares the names of the two directory entries using strcmp and returns the result of the comparison
int nameCompareODir(const struct dirent **d1, const struct dirent **d2) {
    return strcmp((*d1)->d_name, (*d2)->d_name);
}

// Function to manage errors
void manageerror(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}


// Function prototypes
void performdirlista(int client_socket);
void performdirlistt(int client_socket);
void performw24fn(int client_socket, char *filename);
int filecopying(const char *source_path, const char *destination_path);
void performw24fz(int client_socket, long size1, long size2);
void performw24fdb(int client_socket, char *date);
void performw24fda(int client_socket, char *date);
void performw24ft(int client_socket, char *extensions[], int ext_count);
void sendToMirror1(int client_socket, const char *command);
void sendToMirror2(int client_socket, const char *command);



// Function to determine redirection destination based on connection count
char *redirect_destination(int connection_count) {
    if (connection_count <= 3) {
        printf("Server");
        return NULL;  
        // using serverw24 for this only 
    } else if (connection_count > 3 && connection_count < 7) {
        printf("M1");
        return "Mirror1";
    } else if (connection_count > 6 && connection_count < 10) {
        printf("M2");
        return "Mirror2";
    } else {
// after the first 9 connections, it will alternate among serverw24, mirror1 and mirror2 vased on the remaining_connections value 
        int remaining_connections = connection_count - 9;
        if (remaining_connections % 3 == 1) {
            return "Serverw24";
        } else if (remaining_connections % 3 == 2) {
            return "Mirror1";
        } else {
            return "Mirror2";
        }
    }
}

// Function to manage client commands
void manage_command(int client_socket, const char *command) {
    // Check if the command is "w24fn"
    if (strncmp(command, "w24fn", 5) == 0) {
        // Extract filename from command
        const char *filename = command + 6;
        // manage w24fn command
        performw24fn(client_socket, filename);
        return; // Exit function after handling w24fn command
    } else if (strncmp(command, "w24fda", 6) == 0) {
        // Extract date from command
        const char *date_str = command + 6;
        // manage w24fda command
        performw24fda(client_socket, date_str);
        return; // Exit function after handling w24fda command
    } else if (strncmp(command, "w24fz ", 5) == 0) {
        // Extract size range from command
        long size1, size2;
        if (sscanf(command + 6, "%ld %ld", &size1, &size2) != 2) {
            send(client_socket, "Invalid size range format", strlen("Invalid size range format"), 0);
            return;
        }
        // manage w24fz command
        performw24fz(client_socket, size1, size2);
        return; // Exit function after handling w24fz command
    } else if (strncmp(command, "w24fdb", 6) == 0) {
        const char *date_str = command + 7;
        performw24fdb(client_socket, date_str);
        return; // Exit function after handling w24fdb command
    } else if (strncmp(command, "w24ft", 5) == 0) {
        // Adjust the command pointer to point to the extensions
        command += 6;        
        // Extract up to three extensions
        char *extensions[3];
        int ext_count = 0;
        char *token = strtok(command, " ");
        while (token != NULL && ext_count < 3) {
            extensions[ext_count++] = token;
            token = strtok(NULL, " ");
        }
        if (ext_count == 0) {
            printf("[DEBUG] Invalid command\n");
            close(client_socket);
            return;
        }
        performw24ft(client_socket, extensions, ext_count);
        printf("[DEBUG] w24ft function called\n");
        return; // Exit function after handling w24ft command
    }
    // Process other commands
    if (strcmp(command, "dirlist -a") == 0) {
        // manage dirlist -a command
        performdirlista(client_socket);
    } else if (strcmp(command, "dirlist -t") == 0) {
        // manage dirlist -t command
        performdirlistt(client_socket);
    } else if (strcmp(command, "quitc") == 0) {
        // manage quitc command
        return; // Exit function after handling quitc command
    } else {
        printf("Invalid commands");
    }
}

void send_response(int client_socket, const char *response) {
    if (send(client_socket, response, strlen(response), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }
}
 
int compare_entries(const struct dirent **a, const struct dirent **b) {
    return strcasecmp((*a)->d_name, (*b)->d_name);
}

void performdirlista(int client_socket) {
    printf("Listing directories and subdirectories alphabetically...\n");
 
    // Get the home directory
    char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        perror("getenv");
        exit(EXIT_FAILURE);
    }
 
    // Start listing directories recursively from the home directory
    list_directories_recursive(client_socket, home_dir);
 
    // Send a termination message to indicate the end of data
    send_response(client_socket, "EndOfData\n");
}
 
void list_directories_recursive(int client_socket, const char *path) {
    DIR *dir;
    struct dirent **namelist;
    int n;
 
    // Open the directory
    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        return;
    }
 
    // Scan the directory for subdirectories
    n = scandir(path, &namelist, NULL, compare_entries);
    if (n == -1) {
        perror("scandir");
        return;
    }
 
    // Concatenate directory names into a single buffer
    char response[MAXDATASIZE];
    response[0] = '\0'; // Ensure the buffer is initially empty
    for (int i = 0; i < n; i++) {
        if (namelist[i]->d_type == DT_DIR) {
            // Skip "." and ".." entries
            if (strcmp(namelist[i]->d_name, ".") != 0 && strcmp(namelist[i]->d_name, "..") != 0) {
                strcat(response, namelist[i]->d_name);
                strcat(response, "\n");
            }
        }
        free(namelist[i]);
    }
    free(namelist);
 
    // Send the concatenated buffer to the client
    if (send(client_socket, response, strlen(response), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }
 
    closedir(dir);
}


// Function to manage dirlist -t command
void performdirlistt(int client_socket) {
    char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        perror("getenv");
        exit(EXIT_FAILURE);
    }

    // Array to hold directory information
    DirInfo dirs[MAX_DIRS];
    int num_dirs = 0;

    DIR *dir = opendir(home_dir); // Open home directory
    if (!dir) {
        perror("opendir");
        return;
    }

    // Read directory entries
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && num_dirs < MAX_DIRS) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // Get directory information
            struct stat statbuf;
            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "%s/%s", home_dir, entry->d_name);
            if (stat(path, &statbuf) == 0) {
                strcpy(dirs[num_dirs].name, entry->d_name);
                dirs[num_dirs].creation_time = statbuf.st_ctime;
                num_dirs++;
            }
        }
    }
    closedir(dir);

    // Sort directories by creation time
    qsort(dirs, num_dirs, sizeof(DirInfo), creationTimecompare);

    // Prepare the directory list as a single message
    char directory_list[BUFFER_SIZE];
    int offset = 0;
    for (int i = 0; i < num_dirs; i++) {
        int len = strlen(dirs[i].name);
        memcpy(directory_list + offset, dirs[i].name, len);
        offset += len;
        directory_list[offset++] = '\n'; // Add newline separator
    }
    directory_list[offset] = '\0'; // Null-terminate the string

    // Send the complete directory list to the client
    if (send(client_socket, directory_list, strlen(directory_list), 0) == -1) {
        perror("send");
        return;
    }
}



// Modify the performw24fn function to manage the w24fn command
void performw24fn(int client_socket, char *filename) {
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s/%s", getenv("HOME"), filename);

    // Open file
    FILE *file = fopen(path, "r");
    if (file != NULL) {
        // Get file size
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Get file permissions
        struct stat file_stat;
        stat(path, &file_stat);
        char permissions[10];
        snprintf(permissions, 10, "%o", file_stat.st_mode);

        // Get file creation time
        char created_time[20];
        strftime(created_time, 20, "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_ctime));

        // Send file information to client
        char info[BUFFER_SIZE];
        snprintf(info, BUFFER_SIZE, "%s Size: %ld bytes, Created: %s, Permissions: %s", filename, size, created_time, permissions);
        if (send(client_socket, info, strlen(info), 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }

        fclose(file);
    } else {
        // Send "File not found" message to client
        if (send(client_socket, "File not found\n", 15, 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
    }
}


int filecopying(const char *source_path, const char *destination_path) {
    FILE *source_file = fopen(source_path, "rb");
    if (source_file == NULL) {
        perror("Error opening source file");
        return -1;
    }
 
    FILE *destination_file = fopen(destination_path, "wb");
    if (destination_file == NULL) {
        perror("Error opening destination file");
        fclose(source_file);
        return -1;
    }
 
    // Copy data from source file to destination file
    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, destination_file) != bytes_read) {
            perror("Error writing to destination file");
            fclose(source_file);
            fclose(destination_file);
            return -1;
        }
    }
 
    // Close files
    fclose(source_file);
    fclose(destination_file);
 
    return 0;
}
void performw24fz(int client_socket, long size1, long size2) {
    const char *w24project_path = "./w24project";
    //Create w24project directory if it doesn't exist
    if (mkdir(w24project_path, PERMISSIONS) == -1 && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
 
    printf("Handling w24fz command...\n");
 
    if (size1 < 0 || size2 < 0 || size1 > size2) {
        // Invalid size range
        printf("Invalid size range\n");
        send(client_socket, "Invalid size range", strlen("Invalid size range"), 0);
        return;
    }
 
    char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        perror("getenv");
        exit(EXIT_FAILURE);
    }
 
    DIR *dir = opendir(home_dir);
    if (dir == NULL) {
        perror("opendir");
        send(client_socket, "Error opening directory", strlen("Error opening directory"), 0);
        return;
    }
 
    printf("Home directory: %s\n", home_dir);
 
    // Create a temporary directory for storing files within the size range
    char temp_dir[] = "./w24fz_temp";
    if (mkdir(temp_dir, 0777) == -1 && errno != EEXIST) {
        perror("mkdir");
        send(client_socket, "Error creating temporary directory", strlen("Error creating temporary directory"), 0);
        closedir(dir);
        return;
    }
 
    printf("Temporary directory created: %s\n", temp_dir);
 
    // Flag to indicate if any files matching the size range were found
    int files_found = 0;
 
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s/%s", home_dir, entry->d_name);
 
        struct stat statbuf;
        if (stat(path, &statbuf) == -1) {
            perror("stat");
            continue;
        }
 
        if (S_ISREG(statbuf.st_mode) && statbuf.st_size >= size1 && statbuf.st_size <= size2) {
            files_found = 1;
            printf("Matching file found: %s\n", entry->d_name);
 
            // Copy the file to the temporary directory
            char dest_path[PATH_MAX];
            snprintf(dest_path, PATH_MAX, "%s/%s", temp_dir, entry->d_name);
            if (filecopying(path, dest_path) == -1) {
                perror("filecopying");
                continue;
            }
            printf("File copied to temporary directory: %s\n", dest_path);
        }
    }
 
    closedir(dir);
 
    if (!files_found) {
        // No files found in the specified size range
        printf("No files found in the specified size range\n");
        send(client_socket, "No file found", strlen("No file found"), 0);
        return;
    }
 
    // Create a tar file from the temporary directory
    char tar_cmd[BUFFER_SIZE];
    snprintf(tar_cmd, BUFFER_SIZE, "tar -czf w24project/temp.tar.gz -C %s .", temp_dir);
    printf("Executing tar command: %s\n", tar_cmd);
 
    int status = system(tar_cmd);
    if (status == -1) {
        perror("system");
        send(client_socket, "Error creating tar file", strlen("Error creating tar file"), 0);
        return;
    } else if (status != 0) {
        fprintf(stderr, "Error creating tar file\n");
        send(client_socket, "Error creating tar file", strlen("Error creating tar file"), 0);
        return;
    }
 
    printf("Tar command executed successfully.\n");
 
    // Send the path of the created tar file to the client
    if (send(client_socket, "temp.tar.gz", strlen("temp.tar.gz"), 0) == -1) {
        perror("send");
        return;
    }
}

void performw24fdb(int client_socket, char *date) {
    const char *w24project_path = "./w24project";
    const char *w24fdb_temp_path = "./w24project/w24fdb_temp";
    const char *tar_file = "temp.tar.gz";
    // Check if the date argument is provided
    if (date == NULL) {
        if (send(client_socket, "No date provided", strlen("No date provided"), 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
        return;
    }
    // Create w24project directory if it doesn't exist
    if (mkdir(w24project_path, PERMISSIONS) == -1 && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
 
    // Create w24fdb_temp directory inside w24project
    if (mkdir(w24fdb_temp_path, PERMISSIONS) == -1 && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
 
    printf("Directories created successfully.\n");
 
    // Construct the find command to search for files modified or created after the provided date
    char find_cmd[BUFFER_SIZE];
    snprintf(find_cmd, BUFFER_SIZE, "find ~ -type f -not -newermt \"%s\" -exec cp -n -t %s {} +", date, w24fdb_temp_path);
    printf("Executing find command: %s\n", find_cmd); // Debugging statement
 
    // Execute the find command
    int status = system(find_cmd);
    if (status == -1) {
        perror("system");
        exit(EXIT_FAILURE);
    } 
    else if (status != 0) {
    fprintf(stderr, "Error executing find command: %s\n", strerror(errno));
    exit(EXIT_FAILURE);}
 
else {
    printf("Find command executed successfully.\n");
}
 
struct stat st;
    if (stat(tar_file, &st) == 0) {
        // Tar file exists, delete it
        if (remove(tar_file) == -1) {
            perror("remove");
            exit(EXIT_FAILURE);
        }
    }
char tar_cmd[BUFFER_SIZE];
    snprintf(tar_cmd, BUFFER_SIZE, "tar -czf w24project/temp.tar.gz -C %s .", w24fdb_temp_path);
    printf("Executing tar command: %s\n", tar_cmd); // Debugging statement
 
    // Execute the tar command
    status = system(tar_cmd);
    if (status == -1) {
        perror("system");
        exit(EXIT_FAILURE);
    } else if (status != 0) {
        fprintf(stderr, "Error creating tar file\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Tar command executed successfully.\n");
    }
 
    // Send the path of the created tar file to the client
    if (send(client_socket, "temp.tar.gz", strlen("temp.tar.gz"), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }
 
 
}


void performw24fda(int client_socket, char *date) {
    const char *w24project_path = "./w24project";
    const char *w24fda_temp_path = "./w24project/w24fda_temp";
    const char *tar_file = "temp.tar.gz";
    // Check if the date argument is provided
    if (date == NULL) {
        if (send(client_socket, "No date provided", strlen("No date provided"), 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
        return;
    }
   // Create w24project directory if it doesn't exist
    if (mkdir(w24project_path, PERMISSIONS) == -1 && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }

    // Create w24fdb_temp directory inside w24project
    if (mkdir(w24fda_temp_path, PERMISSIONS) == -1 && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }

    printf("Directories created successfully.\n");
    
    // Construct the find command to search for files modified or created after the provided date
    char find_cmd[BUFFER_SIZE];
    snprintf(find_cmd, BUFFER_SIZE, "find ~ -type f -newermt \"%s\" -exec cp -n -t %s {} +", date, w24fda_temp_path);
    printf("Executing find command: %s\n", find_cmd); // Debugging statement

    // Execute the find command
    int status = system(find_cmd);
    if (status == -1) {
        perror("system");
        exit(EXIT_FAILURE);
    } 
    else if (status != 0) {
    fprintf(stderr, "Error executing find command: %s\n", strerror(errno));
    exit(EXIT_FAILURE);}

 else {
    printf("Find command executed successfully.\n");
}
  char tar_cmd[BUFFER_SIZE];
    snprintf(tar_cmd, BUFFER_SIZE, "tar -czf w24project/temp.tar.gz -C %s .", w24fda_temp_path);
    printf("Executing tar command: %s\n", tar_cmd); // Debugging statement

    // Execute the tar command
    status = system(tar_cmd);
    if (status == -1) {
        perror("system");
        exit(EXIT_FAILURE);
    } else if (status != 0) {
        fprintf(stderr, "Error creating tar file\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Tar command executed successfully.\n");
    }

    // Send the path of the created tar file to the client
    if (send(client_socket, "temp.tar.gz", strlen("temp.tar.gz"), 0) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

}


// Function to send and receive data
void send_receive(int client_socket, const char *command) {
    // Send command to server
    if (send(client_socket, command, strlen(command), 0) < 0) {
        manageerror("Error sending data to server");
    }

    if (strcmp(command, "quitc") != 0) {
        char buffer[BUFFER_SIZE];
        // Receive and print list of directories or file information from server
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received < 0) {
            manageerror("Error receiving data from server");
        } else if (bytes_received == 0) {
            printf("Server closed connection\n");
            exit(EXIT_SUCCESS);
        }

        buffer[bytes_received] = '\0';
        printf("%s\n", buffer); // Print received data

        // Check if the command is "w24fz" and create the w24project directory if it doesn't exist
        if (strncmp(buffer, "w24fz", 5) == 0) {
            system("mkdir -p ~/w24project");
            printf("Temporary tar file created.\n");
            // Move the temporary tar file to w24project directory
            if (system("mv /tmp/w24fda_temp/temp.tar.gz ~/w24project/") == -1) {
                manageerror("Error moving temp.tar.gz to w24project directory");
            }
        }
    }
}

void performw24ft(int client_socket, char *extensions[], int ext_count) {
   printf("Handling w24ft command...\n");
   const char *w24project_path = "./w24project";
    //Create w24project directory if it doesn't exist
    if (mkdir(w24project_path, PERMISSIONS) == -1 && errno != EEXIST) {
        perror("mkdir");
        exit(EXIT_FAILURE);
    }
   // Check if the number of extensions is valid
   if (ext_count < 1 || ext_count > 3) {
       printf("Invalid number of extensions. Provide 1 to 3 extensions.\n");
       send(client_socket, "Invalid number of extensions. Provide 1 to 3 extensions.", strlen("Invalid number of extensions. Provide 1 to 3 extensions."), 0);
       return;
   }
   // Construct the find command to search for files matching the specified extensions
   char find_command[BUFFER_SIZE];
   snprintf(find_command, sizeof(find_command), "find ~ -type f \\( ");
   for (int i = 0; i < ext_count; i++) {
       snprintf(find_command + strlen(find_command), sizeof(find_command) - strlen(find_command), "-name \"*.%s\"", extensions[i]);
       if (i < ext_count - 1) {
           snprintf(find_command + strlen(find_command), sizeof(find_command) - strlen(find_command), " -o ");
       }
   }
   strcat(find_command, " \\)");
   printf("Find command: %s\n", find_command);
   // Execute the find command
   FILE *find_output = popen(find_command, "r");
   if (!find_output) {
       perror("Error executing find command");
       send(client_socket, "Error executing find command", strlen("Error executing find command"), 0);
       return;
   }
   // Create a temporary file list
   char temp_file[] = "w24project/w24ft_temp_list.txt";
   FILE *temp_file_ptr = fopen(temp_file, "w");
   if (!temp_file_ptr) {
       perror("Error creating temporary file list");
       pclose(find_output);
       return;
   }
   // Read the output of the find command and write each file path to the temporary file list
   char file_path[BUFFER_SIZE];
   while (fgets(file_path, sizeof(file_path), find_output) != NULL) {
       fprintf(temp_file_ptr, "%s", file_path);
   }
   // Close the temporary file list
   fclose(temp_file_ptr);
   // Close the find command output
   pclose(find_output);
   // Create a tar archive from the files listed in the temporary file list
   char tar_command[BUFFER_SIZE];
    snprintf(tar_command, sizeof(tar_command), "tar -czf w24project/temp.tar.gz -C w24project -T w24project/w24ft_temp_list.txt");
   printf("Tar command: %s\n", tar_command);
   int ret = system(tar_command);
   if (ret == -1) {
       perror("Error creating tar archive");
       send(client_socket, "Error creating tar archive", strlen("Error creating tar archive"), 0);
   } else {
       printf("Tar archive created successfully.\n");
       send(client_socket, "Tar archive created successfully", strlen("Tar archive created successfully"), 0);
   }
   // Remove the temporary file list
   remove(temp_file);
}

// Function to forward client command to Mirror1
void sendToMirror1(int client_socket, const char *command) {
    struct sockaddr_in mirror1_addr;
    int mirror1_socket;

    // Create socket for Mirror1
    if ((mirror1_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Mirror1 socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize Mirror1 address structure
    memset(&mirror1_addr, 0, sizeof(mirror1_addr));
    mirror1_addr.sin_family = AF_INET;
    mirror1_addr.sin_port = htons(MIRROR1_PORT);
    mirror1_addr.sin_addr.s_addr = inet_addr(MIRROR1_IP);

    // Connect to Mirror1
    printf("Connecting to Mirror1...\n"); // Debug statement
    if (connect(mirror1_socket, (struct sockaddr *)&mirror1_addr, sizeof(mirror1_addr)) == -1) {
        perror("Mirror1 connection failed");
        exit(EXIT_FAILURE);
    }

    // Send client's command to Mirror1
    printf("Sending command to Mirror1: %s\n", command); // Debug statement
    send(mirror1_socket, command, strlen(command), 0);

    // Receive response from Mirror1
    receive_response_from_mirror(client_socket, mirror1_socket);

    // Close connection to Mirror1
    close(mirror1_socket);
}

// Function to forward client command to Mirror2
void sendToMirror2(int client_socket, const char *command) {
    struct sockaddr_in mirror2_addr;
    int mirror2_socket;

    // Create socket for Mirror2
    if ((mirror2_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Mirror2 socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize Mirror2 address structure
    memset(&mirror2_addr, 0, sizeof(mirror2_addr));
    mirror2_addr.sin_family = AF_INET;
    mirror2_addr.sin_port = htons(MIRROR2_PORT);
    mirror2_addr.sin_addr.s_addr = inet_addr(MIRROR2_IP);

    // Connect to Mirror2
    printf("Connecting to Mirror2...\n"); // Debug statement
    if (connect(mirror2_socket, (struct sockaddr *)&mirror2_addr, sizeof(mirror2_addr)) == -1) {
        perror("Mirror2 connection failed");
        exit(EXIT_FAILURE);
    }

    // Send client's command to Mirror2
    printf("Sending command to Mirror2: %s\n", command); // Debug statement
    send(mirror2_socket, command, strlen(command), 0);

    // Receive response from Mirror2
    receive_response_from_mirror(client_socket, mirror2_socket);

    // Close connection to Mirror2
    close(mirror2_socket);
}

// Function to receive response from Mirror servers and send it to the client
void receive_response_from_mirror(int client_socket, int mirror_socket) {
    char buffer[MAXDATASIZE];
    ssize_t bytes_received;

    // Receive response from Mirror server
    printf("Receiving response from Mirror server...\n"); // Debug statement
    bytes_received = recv(mirror_socket, buffer, MAXDATASIZE, 0);
    if (bytes_received == -1) {
        perror("Receive from mirror failed");
        close(client_socket);
        return;
    } else if (bytes_received == 0) {
        printf("Mirror server closed the connection\n");
        return;
    }

    // Send Mirror's response back to the client
    printf("Sending Mirror's response to client...\n"); // Debug statement
    if (send(client_socket, buffer, bytes_received, 0) == -1) {
        perror("Send to client failed");
        close(client_socket);
        return;
    }
}


void crequest(int client_socket, int connection_count) {
    char buffer[MAXDATASIZE];
    int num_bytes_recv;

    while (1) {
        num_bytes_recv = recv(client_socket, buffer, MAXDATASIZE, 0);
        if (num_bytes_recv <= 0) {
            break;
        }
        buffer[num_bytes_recv] = '\0';

        // Redirect based on connection count
        char *destination = redirect_destination(connection_count);
        printf("Destination: %s\n", destination);
        if (destination != NULL) {
            // manage redirection
            if (strcmp(destination, "Mirror1") == 0) {
                sendToMirror1(client_socket, buffer);
            } else if (strcmp(destination, "Mirror2") == 0) {
                sendToMirror2(client_socket, buffer);
            } else {
                // No redirection required, manage command directly
                manage_command(client_socket, buffer);
            }
        } else {
            // No redirection required, manage command directly
            manage_command(client_socket, buffer);
        }
    }
    close(client_socket);
}


int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t sin_size;
    int pid;
    int connection_count = 1;

    // Create socket
    printf("Creating socket...\n"); // Debug statement
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // Server address setup
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(server_addr.sin_zero), '\0', 8);

    // Bind socket
    printf("Binding socket...\n"); // Debug statement
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("Bind failed");
        exit(1);
    }

    // Listen for connections
    printf("Listening for connections...\n"); // Debug statement
    if (listen(server_socket, BACKLOG) == -1) {
        perror("Listen failed");
        exit(1);
    }

    printf("Serverw24 is listening on port %d...\n", PORT);

    while (1) {
        sin_size = sizeof(struct sockaddr_in);
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
            perror("Accept failed");
            continue;
        }

        printf("Connection from %s has been established!\n", inet_ntoa(client_addr.sin_addr));
        printf("Connection count: %d\n", connection_count);

        // Fork child process to manage client request
        pid = fork();
        if (pid == 0) { // Child process
            close(server_socket); // Close server socket in child process
            crequest(client_socket, connection_count); // manage client request
            exit(0); // Terminate child process
        } else if (pid > 0) { // Parent process
            close(client_socket); // Close client socket in parent process
            connection_count++;
        } else {
            perror("Fork failed");
            exit(1);
        }
    }

    return 0;
}


