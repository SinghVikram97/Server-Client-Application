#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <dirent.h>

void listSubdirectories(char *buffer) {
    char *path = getenv("HOME");
    DIR *dir;
    struct dirent *entry;
    size_t buffer_length = 0;
    
    // Open directory
    if ((dir = opendir(path)) != NULL) {
        // Iterate through directory entries
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR) {
                // Skip "." and ".."
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;
                
                // Append directory name to buffer
                size_t entry_length = strlen(entry->d_name);
                if (buffer_length + entry_length + 2 <= 1024) {
                    strcat(buffer, entry->d_name);
                    strcat(buffer, "\n");
                    buffer_length += entry_length + 1;
                } else {
                    fprintf(stderr, "Buffer overflow\n");
                    break;
                }
            }
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