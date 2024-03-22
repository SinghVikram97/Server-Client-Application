#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>

void crequest(int count, int connfd){
    char buffer[255];
    int n;
    while(1){
        bzero(buffer,255);
        n = read(connfd, buffer, 255);

        if(n<0){
            printf("Error on reading\n");
        }

        printf("Message from client number %d: %s\n",count,buffer);

        // fgets(buffer, 255, stdin);

        // n = write(connfd, buffer, strlen(buffer));

        // if(n<0){
        //     printf("Error on writing\n");
        // }

        int i = strncmp("quitc", buffer, 3); 

        if(i==0) {
            break;
        }
    }
    close(connfd);   
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