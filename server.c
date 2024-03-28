#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <dirent.h>

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

void crequest(int count, int connfd){
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
        else if(strncmp("dirlist -a",buffer,strlen("dirlist -a"))==0){
            bzero(buffer,1024);
            listSubdirectories(buffer);
            n = write(connfd, buffer, strlen(buffer));
            if(n<0){
                printf("Error on writing\n");
            }

        }
        else{
            printf("Message from client number %d: %s\n",count,buffer);
            bzero(buffer,1024);
            strcpy(buffer, "message received on server side no handler yet\n");
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