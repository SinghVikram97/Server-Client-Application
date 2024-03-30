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
#include <fcntl.h>
#include <sys/wait.h>
#include <libgen.h>
#include <ctype.h>

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
void copy_file(const char *source, const char *destination_with_filename) {
    // Open the source file in readonly mode
    int source_fd = open(source, O_RDONLY);
    if (source_fd == -1) {
        printf("Error opening source file\n");
        return;
    }

    // Find the last '/' in the source path
    const char *last_slash = strrchr(source, '/');
    const char *file_name = (last_slash != NULL) ? last_slash + 1 : source;

    // Append the file name to the destination path
    char destination_path[strlen(destination_with_filename) + strlen(file_name) + 2]; // +2 for '/' and null terminator
    sprintf(destination_path, "%s/%s", destination_with_filename, file_name);

    if(strcmp(source,destination_path)==0)
    {
        return;
    }
    // Create or open the destination file with appropriate permissions
    int destination_fd = open(destination_path, O_CREAT | O_WRONLY | O_TRUNC, 0744);
    if (destination_fd == -1) {
        printf("Error creating/opening destination file\n");
        close(source_fd);
        return;
    }

    char buffer[1024];
    ssize_t bytes_read;

    // Read from source and write to destination
    while ((bytes_read = read(source_fd, buffer, sizeof(buffer))) > 0) {
        if (write(destination_fd, buffer, bytes_read) != bytes_read) {
            printf("Error writing to destination file\n");
            close(source_fd);
            close(destination_fd);
            return;
        }
    }

    if (bytes_read == -1) {
        printf("Error reading from source file\n");
    }

    // Close file descriptors
    close(source_fd);
    close(destination_fd);
}

void create_tar_gz(const char *folder) {
    char command[1024];
    snprintf(command, sizeof(command), "tar -czf temp.tar.gz -C %s .", folder);
    //system(command);
    char* buffer = execute_command(command);
}

void delete_folder(const char *folder) {
    char command[1024];
    snprintf(command, sizeof(command), "rm -rf \"%s\"", folder);
    char* buffer = execute_command(command);
}

char *get_file_extension(const char *filename) {
    char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return NULL;  // No extension found or filename starts with '.'
    return dot + 1;  // Return the extension (excluding the dot)
}

void get_files(const char *dir_path,long size1,long size2, const char *temp_folder) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char file_path[1024];
    
    dir = opendir(dir_path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

        if (lstat(file_path, &file_stat) == -1) {
            perror("lstat");
            continue;
        }

        if (S_ISDIR(file_stat.st_mode)) {
            get_files(file_path, size1, size2, temp_folder);
        } else if (S_ISREG(file_stat.st_mode)) {
            char *extension = get_file_extension(entry->d_name);
            if (extension != NULL && file_stat.st_size >= size1 && file_stat.st_size <= size2) {
                //printf("Copying file: %s\n", file_path);
                copy_file(file_path, temp_folder);
            }
        }
    }

    closedir(dir);
}
void extractLongs(char* string, long *size1, long *size2) {
    char *token;
    int count = 0;
    // Skip the first token "w24z"
    token = strtok(string, " ");
    token = strtok(NULL, " ");
    while (token != NULL && count < 2) {
        if (count == 0) {
            *size1 = atol(token);
        } else if (count == 1) {
            *size2 = atol(token);
        }
        token = strtok(NULL, " ");
        count++;
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
           else if(strncmp("w24fz",buffer,strlen("w24fz"))==0){
            long size1, size2;
            extractLongs(buffer, &size1, &size2);

            char temp_folder[1024];
            snprintf(temp_folder, sizeof(temp_folder), "%s/temp", getenv("HOME"));

            // Create temp folder if it doesn't exist
            mkdir(temp_folder, 0700);

            get_files(getenv("HOME"), size1, size2, temp_folder);

            // Create tar.gz file
            create_tar_gz(temp_folder);

            // Delete temp folder
            delete_folder(temp_folder);
            // n = write(connfd, buffer, strlen(buffer));
            // if(n<0){
            //     printf("Error on writing\n");
            // }
         }
         
        else{
            bzero(buffer,1024);
            strcpy(buffer, "Sending you back my message from Mirror1\n");
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