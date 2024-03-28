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
#include<netdb.h>
#include<netinet/in.h>

#define SERVER_IP "127.0.0.1"
#define MIRROR1_PORT "8073"

// Struct to hold directory name and creation time
// Struct to hold directory information
typedef struct {
    char name[256]; // Directory name
    char creation_time[20]; // Creation time
} DirectoryInfo;

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

void handleRequestOnClient(int count, int connfd, char *buffer){
    int n;
    if(strncmp("dirlist -a",buffer,strlen("dirlist -a"))==0){
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

    }else{
            printf("Message from client number %d: %s\n",count,buffer);
            bzero(buffer,1024);
            strcpy(buffer, "message received on server side no handler yet\n");
            n = write(connfd, buffer, strlen(buffer));
            if(n<0){
                printf("Error on writing\n");
            }
    }
}

void handleRequestOnMirror1(int count, int connfd, int sockfdMirror1, char * buffer) {
    int n;
    // connfd is for client 
    // sockfdMirror1 is for writing to Mirror1
    // bzero(buffer, 1024);
    // strcpy(buffer, "Send Message to Mirror1 %s\n");
    // Write to mirror1
    n = write(sockfdMirror1, buffer, strlen(buffer));

    if (n < 0) {
        printf("Error on writing\n");
    }

    // Read from mirror1
    bzero(buffer, 1024);

    n = read(sockfdMirror1, buffer, 1024);

    if (n < 0) {
        printf("Error on reading\n");
    }

    // Write to client now
    n = write(connfd, buffer, strlen(buffer));
    if (n < 0) {
        printf("Error on writing\n");
    }
}

void crequest(int count, int connfd, int sockfdMirror1){
    char buffer[1024];
    int n;
    while(1){
        bzero(buffer,1024);
        n = read(connfd, buffer, 1024);

        if(n<0){
            printf("Error on reading\n");
        }

        if(strncmp("quitc", buffer, strlen("quitc"))==0){
            printf("Closing connection on server side for client %d\n",count);
            break;
        }

        if(count%2==1){
            handleRequestOnClient(count, connfd, buffer);
        }else{
            handleRequestOnMirror1(count, connfd, sockfdMirror1, buffer);
        }

        bzero(buffer,1024);
    }
    close(connfd);   
    exit(0);
}

int main(int argc, char *argv[]) {
   /* --------  Listen to Client Connections --------*/
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
   /* --------  List to Client Connections End --------*/

   /* --------  Open Socket to Mirror1  --------*/
   
   // Open socket to Mirror1
   int sockfdMirror1, portnoMirror1;
   struct sockaddr_in serv_addr_mirror1;
   struct hostent *server_mirror1;
   
   portnoMirror1 = atoi(MIRROR1_PORT);
   sockfdMirror1 = socket(AF_INET, SOCK_STREAM, 0);
   
   if(sockfdMirror1 < 0) {
        printf("Error opening socket to mirror1\n");
   }

   server_mirror1 = gethostbyname(SERVER_IP);
   
   if(server_mirror1==NULL){
        printf("No such host \n");
   }
   
   serv_addr_mirror1.sin_family = AF_INET;
   bcopy((char *)server_mirror1->h_addr_list[0], (char *) &serv_addr_mirror1.sin_addr.s_addr, server_mirror1->h_length);
   serv_addr_mirror1.sin_port = htons(portnoMirror1);
   
   if(connect(sockfdMirror1, (struct sockaddr *) &serv_addr_mirror1, sizeof(serv_addr_mirror1))<0){
       printf("Connection failed\n");
   }

   /* --------  Open Socket to Mirror1 End --------*/

   int count=1;
   while(1){
      clilen = sizeof(cli_addr);

      connfd = accept(listenfd, (struct sockaddr *) &cli_addr, &clilen);

      if(connfd<0){
        printf("Error on accept");
      }

      pid_t pid = fork();
      if(pid==0){
            crequest(count, connfd, sockfdMirror1);
            exit(0);
      }else{
        count++;
        close(connfd);
      }
      
   }

   close(listenfd);
   return 0;
}