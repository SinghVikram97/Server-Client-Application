#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include <fcntl.h>
#include <sys/stat.h>

void processCommand(int sockfd, char *buffer) {
    int n;
    n = write(sockfd, buffer, strlen(buffer));

    if(n<0){
        printf("Error on writing\n");
    }

    bzero(buffer, 1024);

    n = read(sockfd, buffer, 1024);

    if(n<0){
        printf("Error on reading\n");
    }

    printf("%s\n",buffer);
}

void processFileCommand(int sockfd, char *buffer) {
    int n;
    n = write(sockfd, buffer, strlen(buffer));

    if(n<0){
        printf("Error on writing\n");
    }    

    // read file size
    off_t file_size;
    if (read(sockfd, &file_size, sizeof(file_size)) < 0) {
        perror("Error reading file size from socket");
        return;
    }

    umask(0);
    int fd = open("output_file.txt", O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd == -1) {
        printf("Error opening file");
        return;
    }

    bzero(buffer, 1024);

    ssize_t bytes_received;
    off_t total_bytes_received = 0;
    while (total_bytes_received < file_size &&
           (bytes_received = read(sockfd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_written = write(fd, buffer, bytes_received);
        if (bytes_written < 0) {
            perror("Error writing to file");
            close(fd);
            return;
        }
        total_bytes_received += bytes_written;
    }
    printf("Finished\n");
    close(fd);
    if (bytes_received < 0) {
        perror("Error reading from socket");
    }
}

int main(int argc, char *argv[]) {
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[1024];

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0) {
        printf("Error opening socket\n");
    }

    server = gethostbyname(argv[1]);

    if(server==NULL){
        printf("No such host \n");
    }

    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))<0){
        printf("Connection failed\n");
    }

    while(1){
        bzero(buffer, 1024);

        fgets(buffer, 1024, stdin);

        if(strncmp(buffer,"quitc",strlen("quitc"))==0){
            int n = write(sockfd, buffer, strlen(buffer));
            if(n<0){
                printf("Error on writing\n");
            }
            printf("Closing connection on client side\n");
            break;
        }else if(strncmp(buffer,"dirlist -a",strlen("dirlist -a"))==0){
            processCommand(sockfd, buffer);
        }else if(strncmp(buffer,"dirlist -t",strlen("dirlist -t"))==0){
            processCommand(sockfd, buffer);           
        }else if(strncmp(buffer,"w24fn",strlen("w24fn"))==0){
            processCommand(sockfd, buffer);
        }else if(strncmp(buffer,"w24fz",strlen("w24fz"))==0){
            processCommand(sockfd, buffer);
        }else if(strncmp(buffer,"w24ft",strlen("w24ft"))==0){
            processCommand(sockfd, buffer);
        }else if(strncmp(buffer,"w24fdb",strlen("w24fdb"))==0){
            processCommand(sockfd, buffer);
        }else if(strncmp(buffer,"w24fda",strlen("w24fda"))==0){
            processCommand(sockfd, buffer);
        }else if(strncmp(buffer,"hi",strlen("hi"))==0){
            processCommand(sockfd,buffer);
        }else if(strncmp(buffer,"file",strlen("file"))==0){
            processFileCommand(sockfd, buffer);
        }
        else{
            printf("Invalid command, please try again\n");
        }
    }
    close(sockfd);
    return 0;
}