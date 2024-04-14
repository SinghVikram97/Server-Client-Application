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
#include<netdb.h>
#include<netinet/in.h>

#define MAX_EXTENSIONS 3
#define MAX_PATH_LENGTH 1024

// Structure to hold directory name and creation time
struct DirCreationTime {
    char dirname[1024];
    char creationTime[1024];
};

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

void sendFile(int connfd, char *buffer, int found) {

    if(found==0){
         // Signal that no file transfer
        const char *start_signal = "DONOT_TRANSFER";
        ssize_t start_signal_len = strlen(start_signal);

        int n = write(connfd, start_signal, start_signal_len);
        printf("DONOT_TRANSFER bytes written %d\n",n);
        return;
    }
    

    // Signal the start of file transfer
    const char *start_signal = "START_TRANSFER";
    ssize_t start_signal_len = strlen(start_signal);

    int n = write(connfd, start_signal, start_signal_len);
    printf("START_TRANSFER bytes written %d\n",n);
   

    int fd = open("./temp.tar.gz", O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }

    // Get file size
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    // send file size to client
    if (write(connfd, &file_size, sizeof(file_size)) < 0) {
        perror("Error writing file size to socket");
        close(fd);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, 1024)) > 0) {
        if (write(connfd, buffer, bytes_read) < 0) {
            perror("Error writing to socket");
            close(fd);
            exit(EXIT_FAILURE);
        }
    }
    printf("Finished\n");
    close(fd);
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
void copy_file(const char *source, const char *destination_with_filename, int *found) {
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

    *found=1;

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

void get_files(const char *dir_path,long size1,long size2, const char *temp_folder, int *found) {
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
            get_files(file_path, size1, size2, temp_folder, found);
        } else if (S_ISREG(file_stat.st_mode)) {
            char *extension = get_file_extension(entry->d_name);
            if (extension != NULL && file_stat.st_size >= size1 && file_stat.st_size <= size2) {
                //printf("Copying file: %s\n", file_path);
                copy_file(file_path, temp_folder, found);
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
void copyFilesWithExtensions(const char *directory, const char *extensions[], int numExtensions, int * found) {
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;
    char filepath[MAX_PATH_LENGTH];
    char tempFolder[MAX_PATH_LENGTH];

    // Get home directory
    const char *homeDir = getenv("HOME");
    if (homeDir == NULL) {
        perror("Error getting home directory");
        return;
    }

    // Construct temp folder path
    snprintf(tempFolder, sizeof(tempFolder), "%s/temp", homeDir);

    // Open directory
    if ((dir = opendir(directory)) == NULL) {
        perror("Error opening directory");
        return;
    }

    // Debug point: indicate when a directory is being opened
   // printf("Opening directory: %s\n", directory);

    // Iterate over directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct full file path
        snprintf(filepath, sizeof(filepath), "%s/%s", directory, entry->d_name);

        // Get file information
        if (lstat(filepath, &fileStat) < 0) {
            perror("Error getting file information");
            continue;
        }

        // Check if it's a regular file and has an extension
        if (S_ISREG(fileStat.st_mode)) {
            char *fileExtension = strrchr(entry->d_name, '.');
            if (fileExtension != NULL) {
                // Check if the file extension matches any of the given extensions
                for (int i = 0; i < numExtensions; i++) {
                    // Add period to extension if not present
                    char extensionWithPeriod[MAX_PATH_LENGTH];
                    if (extensions[i][0] == '.') {
                        strcpy(extensionWithPeriod, extensions[i]);
                    } else {
                        snprintf(extensionWithPeriod, sizeof(extensionWithPeriod), ".%s", extensions[i]);
                    }

                    if (strcmp(fileExtension, extensionWithPeriod) == 0) {
                        // Debug point: indicate when a matching file is found
                       // printf("Found matching file: %s\n", filepath);

                        // Copy the file to the temp folder
                        char tempFilePath[MAX_PATH_LENGTH * 2]; // Increased buffer size
                        snprintf(tempFilePath, sizeof(tempFilePath), "%s/temp", getenv("HOME"));
                        *found=1;
                        //snprintf(tempFilePath, sizeof(tempFilePath), "%s/%s", tempFolder, entry->d_name);
                        copy_file(filepath, tempFilePath, found);
                      
                    }
                }
            }
        } else if (S_ISDIR(fileStat.st_mode)) {
            // Debug point: indicate when a directory is being recursively processed
            //printf("Recursively processing directory: %s\n", filepath);
            // Recursively call the function for subdirectories
            copyFilesWithExtensions(filepath, extensions, numExtensions,found);
        }
    }

    closedir(dir);
}
void extractExtensions(const char *buffer, const char *extensions[MAX_EXTENSIONS], int *numExtensions) {
    const char *token;
    *numExtensions = 0;

    // Tokenize the buffer string using space as delimiter
    token = strtok((char *)buffer, " ");
    
    while (token != NULL && *numExtensions < MAX_EXTENSIONS) {
        // Skip "w24ft" prefix if present
        if (strcmp(token, "w24ft") == 0) {
            token = strtok(NULL, " ");
            continue;
        }
        extensions[(*numExtensions)++] = token;
        token = strtok(NULL, " ");
    }
}
void tokenizeString(const char *input, const char *delimiter, const char **tokens, int *numTokens) {
    char *inputCopy = strdup(input); // Make a copy of the input string
    char *token = strtok(inputCopy, delimiter);
    
    *numTokens = 0; // Initialize the number of tokens
    
    while (token != NULL && *numTokens < MAX_EXTENSIONS) {
        // Exclude "w24ft" token
        if (strcmp(token, "w24ft") != 0) {
            // Allocate memory for the token and copy it
            tokens[*numTokens] = strdup(token);
            (*numTokens)++; // Increment the number of tokens
        }
        token = strtok(NULL, delimiter); // Move to the next token
    }
    
    free(inputCopy); // Free the dynamically allocated memory
}
void tokenize_and_remove(char *buffer, char *removeString, char *result) {
    // Tokenize the buffer using strtok
    char *token = strtok(buffer, " ");
    int pos = 0;

    // Iterate through the tokens
    while (token != NULL) {
        // If the token is not the removeString, concatenate it to the result
        if (strcmp(token, removeString) != 0) {
            // Copy token to result
            strcpy(&result[pos], token);
            pos += strlen(token);
            result[pos++] = ' '; // Add space
        }
        token = strtok(NULL, " ");
    }

    // Remove trailing space if exists
    if (pos > 0) {
        result[pos - 1] = '\0';
    }
}
// Function to convert date string to UNIX timestamp
time_t convert_to_timestamp(const char *date_str) {
    struct tm tm = {0};
    if (sscanf(date_str, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) != 3) {
        printf("Error parsing date\n");
        exit(EXIT_FAILURE);
    }
    tm.tm_year -= 1900; // Adjust year
    tm.tm_mon--;        // Adjust month
    return mktime(&tm);
}

// Recursive function to copy files based on creation date condition
void copy_files_recursive(const char *user_date, const char *temp_folder_path, const char *dir_path, int * found, char * type) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        printf("Error opening directory %s\n", dir_path);
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') { // Ignore hidden files and directories
            char entry_path[512];
            snprintf(entry_path, sizeof(entry_path), "%s/%s", dir_path, entry->d_name);
            struct stat stat_buf;
            if (stat(entry_path, &stat_buf) == 0 && S_ISDIR(stat_buf.st_mode)) { // Check if entry is a directory
                copy_files_recursive(user_date, temp_folder_path, entry_path, found,type); // Recursively traverse directories
            } else {
                char file_info[1024] = "";
                getFileInfo(entry_path, file_info);
                char file_date[11];
                sscanf(file_info, "Birth: %s", file_date);
                time_t user_timestamp = convert_to_timestamp(user_date);
                time_t file_timestamp = convert_to_timestamp(file_date);
                if(strcmp(type,"w24fda")==0)
                {
                    if (difftime(file_timestamp, user_timestamp) >= 0) {
                    copy_file(entry_path, temp_folder_path, found);
                    }
                }
                else if(strcmp(type,"w24fdb")==0)
                {
                    if (difftime(file_timestamp, user_timestamp) <= 0) {
                    copy_file(entry_path, temp_folder_path, found);
                    }
                }
                
            }
        }
    }
    closedir(dir);
}
// Function to recursively list directories
void listDirs(const char *basePath, struct DirCreationTime **dirArray, int *count) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;

    if (!(dir = opendir(basePath))) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", basePath, entry->d_name);

        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;  // Ignore current and parent directory
            }

            // Ignore hidden directories
            if (entry->d_name[0] == '.') {
                continue;
            }

            // Get directory info and add to the array
            char dirInfo[1024] = "";
            getFileInfo(path, dirInfo);
            struct DirCreationTime *dirInfoStruct = (struct DirCreationTime *)malloc(sizeof(struct DirCreationTime));
            snprintf(dirInfoStruct->dirname, sizeof(dirInfoStruct->dirname), "%s", path);
            snprintf(dirInfoStruct->creationTime, sizeof(dirInfoStruct->creationTime), "%s", dirInfo);
            dirArray[*count] = dirInfoStruct;
            (*count)++;
        }
    }

    closedir(dir);
}

// Comparator function for sorting directory creation time
int compareCreationTime(const void *a, const void *b) {
    const struct DirCreationTime *da = *(const struct DirCreationTime **)a;
    const struct DirCreationTime *db = *(const struct DirCreationTime **)b;
    return strcmp(da->creationTime, db->creationTime);
}

// Function to print directory information
void printDirInformation(struct DirCreationTime **dirArray, int count, char *buffer) {
    // Initialize buffer index
    int bufferIndex = 0;

    // Print in ascending order
    for (int i = 0; i < count; i++) {
        // Format the string into the buffer
        bufferIndex += sprintf(buffer + bufferIndex, "%s\n\n", dirArray[i]->dirname);
        bufferIndex--; // Adjust buffer index to remove extra space
        free(dirArray[i]); // Free dynamically allocated memory
    }
}

// Function to list directories, sort them, and print their information
void processDirs(const char *basePath, char *buffer) {
    struct DirCreationTime *dirArray[1024];
    int count = 0;

    listDirs(basePath, dirArray, &count);

    // Sort the dirArray based on creation time
    qsort(dirArray, count, sizeof(struct DirCreationTime *), compareCreationTime);

    // Print directory information
    printDirInformation(dirArray, count, buffer);
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
            processDirs(getenv("HOME"), buffer);
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

            int found=0;

            get_files(getenv("HOME"), size1, size2, temp_folder, &found);

            if(found==1){
                // Create tar.gz file
                create_tar_gz(temp_folder);

                // Delete temp folder
                delete_folder(temp_folder);
                bzero(buffer,1024);

                sendFile(connfd, buffer, found);
            }else{
                // Delete temp folder
                delete_folder(temp_folder);
                bzero(buffer,1024);

                sendFile(connfd, buffer, found);
            }            
         }
         else if(strncmp("w24ft",buffer,strlen("w24ft"))==0){
            const char *extensions[MAX_EXTENSIONS];  // Array to store pointers to strings
            int numExtensions = 0;  // Variable to keep track of the number of extensions
            if (buffer[strlen(buffer) - 1] == '\n')
            {
                buffer[strlen(buffer) - 1] = '\0';
            }
            // Tokenize the input string based on spaces
            tokenizeString(buffer, " \n", extensions, &numExtensions);

            extractExtensions(buffer, extensions, &numExtensions);
            //printf("%d\n",numExtensions);
            if (numExtensions < 1 || numExtensions > MAX_EXTENSIONS) {
            printf("Error: Number of extensions should be between 1 and 3.\n");
            return;
            }
            char temp_folder[1024];
            snprintf(temp_folder, sizeof(temp_folder), "%s/temp", getenv("HOME"));
            // for(int i=0;i<numExtensions;i++)
            // {
            //     printf("%s\n",extensions[i]);
            // }

            // Start copying files with specified extensions from the home directory  // Create temp folder if it doesn't exist
            mkdir(temp_folder, 0700);

            int found=0;
            copyFilesWithExtensions(getenv("HOME"), extensions, numExtensions, &found);
            
            if(found==1){
                // Create tar.gz file
                create_tar_gz(temp_folder);

                // Delete temp folder
                delete_folder(temp_folder);
                bzero(buffer,1024);

                sendFile(connfd, buffer, found);
            }else{
                // Delete temp folder
                delete_folder(temp_folder);
                bzero(buffer,1024);

                sendFile(connfd, buffer, found);
            }
    }
    else if(strncmp("w24fda",buffer,strlen("w24fda"))==0){
        char user_date[11]; // Size of "YYYY-MM-DD" + '\0'
        tokenize_and_remove(buffer, "w24fda", user_date);
        if (user_date[strlen(user_date) - 1] == '\n')
        {
            user_date[strlen(user_date) - 1] = '\0';
        }
        char temp_folder[1024];
        snprintf(temp_folder, sizeof(temp_folder), "%s/temp", getenv("HOME"));
        mkdir(temp_folder, 0700);
        int found=0;
        char * type ="w24fda";
        copy_files_recursive(user_date, temp_folder, getenv("HOME"), &found, type);
        if(found==1){
            // Create tar.gz file
            create_tar_gz(temp_folder);

            // Delete temp folder
            delete_folder(temp_folder);
            bzero(buffer,1024);
            sendFile(connfd, buffer, found);
            }else{
                // Delete temp folder
            delete_folder(temp_folder);
            bzero(buffer,1024);
            sendFile(connfd, buffer, found);
            }
    }
     else if(strncmp("w24fdb",buffer,strlen("w24fdb"))==0){
        char user_date[11]; // Size of "YYYY-MM-DD" + '\0'
        tokenize_and_remove(buffer, "w24fdb", user_date);
        if (user_date[strlen(user_date) - 1] == '\n')
        {
            user_date[strlen(user_date) - 1] = '\0';
        }
        char temp_folder[1024];
        snprintf(temp_folder, sizeof(temp_folder), "%s/temp", getenv("HOME"));
        mkdir(temp_folder, 0700);
        int found=0;
        char * type ="w24fdb";
        copy_files_recursive(user_date, temp_folder, getenv("HOME"), &found, type);
        if(found==1){
            // Create tar.gz file
            create_tar_gz(temp_folder);

            // Delete temp folder
            delete_folder(temp_folder);
            bzero(buffer,1024);
            sendFile(connfd, buffer, found);
            }else{
                // Delete temp folder
            delete_folder(temp_folder);
            bzero(buffer,1024);
            sendFile(connfd, buffer, found);
            }
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