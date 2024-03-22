#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>

int main(int argc, char *argv[]) {
   int listenfd, connfd, portno, n;
   char buffer[255];

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
   clilen = sizeof(cli_addr);

   connfd = accept(listenfd, (struct sockaddr *) &cli_addr, &clilen);

   if(connfd<0){
        printf("Error on accept");
   }

   while(1){
        bzero(buffer,255);
        n = read(connfd, buffer, 255);

        if(n<0){
            printf("Error on reading\n");
        }

        printf("Client: %s\n",buffer);
        bzero(buffer,255);

        fgets(buffer, 255, stdin);

        n = write(connfd, buffer, strlen(buffer));

        if(n<0){
            printf("Error on writing\n");
        }

        int i=strncmp("bye",buffer,3);

        if(i==0) {
            break;
        }
   }
   close(listenfd);
   close(connfd);
   return 0;
}