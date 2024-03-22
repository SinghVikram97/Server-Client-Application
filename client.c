#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[255];

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
        bzero(buffer, 255);

        fgets(buffer, 255, stdin);

        n = write(sockfd, buffer, strlen(buffer));

        if(n<0){
            printf("Error on writing\n");
        }

        bzero(buffer, 255);

        n = read(sockfd, buffer, 255);

        if(n<0){
            printf("Error on reading\n");
        }

        printf("Server: %s\n",buffer);

        int i=strncmp("bye",buffer,3);

        if(i==0) {
            break;
        }
    }
    close(sockfd);
    return 0;
}