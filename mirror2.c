#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>
// Struct to hold directory name and creation time
// Struct to hold directory information
typedef struct {
    char name[256]; // Directory name
    char creation_time[20]; // Creation time
} DirectoryInfo;


// Function to execute a command and store its output
char* execute_command(const char* command) {
    char* buffer = (char*)malloc(1024 * sizeof(char));
    if (buffer == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    int pipefd[2];
    pid_t pid;
    int status;
    int saved_stdout;
    int saved_stdin;

    // Save original file descriptors
    saved_stdin = dup(STDIN_FILENO);
    saved_stdout = dup(STDOUT_FILENO);

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();

    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { // Child process
        close(pipefd[0]); // Close unused read end of the pipe

        // Redirect stdout to the write end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]); // Close the original write end of the pipe

        // Execute the command
        if (execl("/bin/sh", "sh", "-c", command, NULL) == -1) {
            perror("execl");
            exit(EXIT_FAILURE);
        }
    } else { // Parent process
        close(pipefd[1]); // Close unused write end of the pipe

        // Read the command output from the pipe
        int bytes_read = read(pipefd[0], buffer, 1024 - 1);
        if (bytes_read == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        buffer[bytes_read] = '\0'; // Null-terminate the buffer
        close(pipefd[0]); // Close the read end of the pipe

        // Wait for child process to finish
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child process exited with error\n");
            exit(EXIT_FAILURE);
        }

        // Restore original file descriptors
        dup2(saved_stdin, STDIN_FILENO);
        dup2(saved_stdout, STDOUT_FILENO);
    }

    return buffer;
}

// Function to get file information using stat command
void getFileInfo(const char *filename, char *buffer) {
    char command[512];

    // Construct the stat command
    snprintf(command, sizeof(command), "stat -c 'Birth: %%w' '%s'", filename);

    // Execute the command and store the output
    char *output = execute_command(command);

    // Copy the output to the buffer
    snprintf(buffer + strlen(buffer), 1024 - strlen(buffer), "%s", output);

    free(output);
}

// Function to search for a file recursively
void searchFileRecursive(const char *filename, const char *directory, char *buffer, int *found) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char path[1024];

    dir = opendir(directory);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        
        // Ignore . and .. directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (stat(path, &file_stat) == -1) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(file_stat.st_mode)) {
            // If it's a directory, recursively search inside it
            searchFileRecursive(filename, path, buffer, found);
            if (*found) // If file found in subdirectory, stop the search
                break;
        } else if (strcmp(entry->d_name, filename) == 0) {
            // If it's the desired file, get its information
            snprintf(buffer + strlen(buffer), 1024 - strlen(buffer), "Name: %s\n", path);
            snprintf(buffer + strlen(buffer), 1024 - strlen(buffer), "Size: %ld bytes\n", file_stat.st_size);
            getFileInfo(path, buffer); //birthdate
            snprintf(buffer + strlen(buffer), 1024 - strlen(buffer), "Permissions: %s%s%s%s%s%s%s%s%s%s\n",
                     (S_ISDIR(file_stat.st_mode)) ? "d" : "-",
                     (file_stat.st_mode & S_IRUSR) ? "r" : "-",
                     (file_stat.st_mode & S_IWUSR) ? "w" : "-",
                     (file_stat.st_mode & S_IXUSR) ? "x" : "-",
                     (file_stat.st_mode & S_IRGRP) ? "r" : "-",
                     (file_stat.st_mode & S_IWGRP) ? "w" : "-",
                     (file_stat.st_mode & S_IXGRP) ? "x" : "-",
                     (file_stat.st_mode & S_IROTH) ? "r" : "-",
                     (file_stat.st_mode & S_IWOTH) ? "w" : "-",
                     (file_stat.st_mode & S_IXOTH) ? "x" : "-");
            *found = 1; // Set found flag
            break;
        }
    }

    closedir(dir);
}

// Function to search for a file recursively from the home directory
void searchFile(const char *filename, char *buffer) {
    int found = 0; // Flag to track if file is found
    searchFileRecursive(filename, getenv("HOME"), buffer, &found);

    if (!found) {
        snprintf(buffer + strlen(buffer), 1024 - strlen(buffer), "File not found.\n");
    }
}

// Comparison function for qsort to sort directories based on creation time
int compareDirectories(const void *a, const void *b) {
    DirectoryInfo *dir1 = (DirectoryInfo *)a;
    DirectoryInfo *dir2 = (DirectoryInfo *)b;
    return strcmp(dir1->creation_time, dir2->creation_time);
}

// Function to list subdirectories in the order of their creation time
void listSubdirectoriesByCreationTime(char *buffer) {
    char *path = getenv("HOME");
    DIR *dir;
    struct dirent *entry;
    size_t buffer_length = 0;
    DirectoryInfo directories[1024];
    int num_directories = 0;

    // Open directory
    if ((dir = opendir(path)) != NULL) {
        // Iterate through directory entries
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR) {
                // Skip "." and ".."
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;

                // Get directory path
                char dir_path[1024];
                snprintf(dir_path, sizeof(dir_path), "%s/%s", path, entry->d_name);

                // Get directory's status
                struct stat file_stat;
                if (stat(dir_path, &file_stat) == -1) {
                    fprintf(stderr, "Error getting file status: %s\n", strerror(errno));
                    continue;
                }

                // Convert creation time to string format
                strftime(directories[num_directories].creation_time, sizeof(directories[num_directories].creation_time),
                         "%Y-%m-%d %H:%M:%S", localtime(&(file_stat.st_ctime)));

                // Copy directory name
                strncpy(directories[num_directories].name, entry->d_name, sizeof(directories[num_directories].name) - 1);
                directories[num_directories].name[sizeof(directories[num_directories].name) - 1] = '\0';

                // Increment directory count
                num_directories++;
            }
        }
        closedir(dir);

        // Sort directories based on creation time
        qsort(directories, num_directories, sizeof(DirectoryInfo), compareDirectories);

        // Copy sorted directories to buffer
        for (int i = 0; i < num_directories; i++) {
            // Get length of directory name
            size_t entry_length = strlen(directories[i].name);

            // Check for buffer overflow
            if (buffer_length + entry_length + 2 <= 1024) {
                // Append directory name to buffer
                strcat(buffer, directories[i].name);
                strcat(buffer, "\n");
                buffer_length += entry_length + 1; // Add length of directory name and newline character
            } else {
                fprintf(stderr, "Buffer overflow\n");
                break;
            }
        }
    } else {
        // Unable to open directory
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }
}

int compareStrings(const void *a, const void *b) {
    const char *str1 = *(const char **)a;
    const char *str2 = *(const char **)b;

    // Check if either string starts with a dot
    int dot1 = (str1[0] == '.');
    int dot2 = (str2[0] == '.');

    // Compare strings based on dot presence
    if (!dot1 && !dot2) {
        return strcasecmp(str1, str2);
    } else if (dot1 && dot2) {
        return strcasecmp(str1 + 1, str2 + 1); // Compare the second character
    } else if (dot1) {
        return strcasecmp(str1 + 1, str2); // Compare the second character of a and the first character of b
    } else { // dot2
        return strcasecmp(str1, str2 + 1); // Compare the first character of a and the second character of b
    }
}

void listSubdirectories(char *buffer) {
    char *path = getenv("HOME");
    DIR *dir;
    struct dirent *entry;
    size_t buffer_length = 0;
    char *directories[1024]; // Assuming max 1024 subdirectories

    // Open directory
    if ((dir = opendir(path)) != NULL) {
        int num_dirs = 0;
        
        // Iterate through directory entries
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR) {
                // Skip "." and ".."
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;

                // Store directory name
                directories[num_dirs++] = strdup(entry->d_name);
            }
        }
        
        // Sort directory names
        qsort(directories, num_dirs, sizeof(char*), compareStrings);

        // Append sorted directory names to buffer
        for (int i = 0; i < num_dirs; i++) {
            size_t entry_length = strlen(directories[i]);
            if (buffer_length + entry_length + 2 <= 1024) {
                strcat(buffer, directories[i]);
                strcat(buffer, "\n");
                buffer_length += entry_length + 1;
            } else {
                fprintf(stderr, "Buffer overflow\n");
                break;
            }
            free(directories[i]); // Free allocated memory for directory name
        }
        
        closedir(dir);
    } else {
        // Unable to open directory
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }
}
// Function to extract filename from a given string
char* extract_filename(const char* str) {
    // Find the position of the space character ' ' in the string
    char* space_pos = strchr(str, ' ');
    if (space_pos != NULL) {
        // Calculate the length of the filename
        int filename_length = strlen(space_pos + 1);

        // Allocate memory for the filename
        char* file_name = (char*)malloc((filename_length + 1) * sizeof(char));

        // Copy the filename to the new buffer
        strcpy(file_name, space_pos + 1);
        return file_name;
    } else {
        return NULL; // No space character found
    }
}
void crequest(int count, int connfd){
    char buffer[1024];
    int n;
    while(1){
        bzero(buffer,1024);
        n = read(connfd, buffer, 1024);

        if(n<0){
            printf("Error on reading\n");
        }

        printf("Message recieved from Server %s\n",buffer);

        if(strncmp("quitc", buffer, strlen("quitc"))==0){
            printf("Closing connection on server side for client %d\n",count);
            break;
        }
        else if(strncmp("dirlist -a",buffer,strlen("dirlist -a"))==0){
            bzero(buffer,1024);
            listSubdirectories(buffer);
            n = write(connfd, buffer, strlen(buffer));
            if(n<0){
                printf("Error on writing\n");
            }

        }
        else if(strncmp("dirlist -t",buffer,strlen("dirlist -t"))==0){
            bzero(buffer,1024);
            listSubdirectoriesByCreationTime(buffer);
            n = write(connfd, buffer, strlen(buffer));
            if(n<0){
                printf("Error on writing\n");
            }

        }

         else if(strncmp("w24fn",buffer,strlen("w24fn"))==0){
            char* file_name = extract_filename(buffer);
            if (file_name[strlen(file_name) - 1] == '\n')
            {
                file_name[strlen(file_name) - 1] = '\0';
            }
            // Search for the file recursively from the home directory
            searchFile(file_name, buffer);
            n = write(connfd, buffer, strlen(buffer));
            if(n<0){
                printf("Error on writing\n");
            }
         }
        else{
            bzero(buffer,1024);
            strcpy(buffer, "Sending you back my message from Mirror2\n");
            n = write(connfd, buffer, strlen(buffer));
            if(n<0){
                printf("Error on writing\n");
            }
        }
        bzero(buffer,1024);
    }
    close(connfd);   
    exit(0);
}

int main(int argc, char *argv[]) {
   int listenfd, connfd, portno, n;

   struct sockaddr_in serv_addr, cli_addr;
   socklen_t clilen;

   listenfd = socket(AF_INET, SOCK_STREAM, 0);

   if(listenfd < 0){
      printf("Error opening socket\n");
   }

   portno = atoi(argv[1]);

   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(portno);

   if(bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("Binding failed\n");
   }

   listen(listenfd, 5);
   int count=1;
   while(1){
      clilen = sizeof(cli_addr);

      connfd = accept(listenfd, (struct sockaddr *) &cli_addr, &clilen);

      if(connfd<0){
        printf("Error on accept");
      }

      pid_t pid = fork();
      if(pid==0){
            crequest(count, connfd);
            exit(0);
      }else{
        count++;
        close(connfd);
      }
      
   }

   close(listenfd);
   return 0;
}