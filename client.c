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
#include <time.h>

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

    // Write command to server
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) {
        perror("Error writing to socket");
        return;
    }

    // Read the start signal
    char start_signal[15];
    ssize_t bytes_receivedSignal = read(sockfd, start_signal, 14);
    if (bytes_receivedSignal < 0) {
        perror("Error reading start signal from socket");
        return;
    }
    start_signal[14] = '\0'; // Null-terminate the string
    printf("Received start signal (%ld bytes): %s\n", bytes_receivedSignal, start_signal);

    if (strcmp(start_signal, "DONOT_TRANSFER") == 0) {
        printf("No file found\n");
        return;
    }

    // read file size
    off_t file_size;
    if (read(sockfd, &file_size, sizeof(file_size)) < 0) {
        perror("Error reading file size from socket");
        return;
    }

    // Generate timestamp
    char timestamp[20];
    time_t current_time;
    struct tm *timeinfo;

    // Get current time
    time(&current_time);

    // Convert to local time
    timeinfo = localtime(&current_time);

    // Format timestamp
    strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", timeinfo);

    // Construct the filename with the timestamp
    char filename[50]; // Adjust size as needed
    snprintf(filename, sizeof(filename), "./temp_%s.tar.gz", timestamp);

    umask(0);
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }

    ssize_t bytes_received;
    off_t total_bytes_received = 0;
    while (total_bytes_received < file_size &&
           (bytes_received = read(sockfd, buffer, 1024)) > 0) {
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

int isValidDateFormat(const char *date) {
    // Check if the date string matches the format YYYY-MM-DD
    if (strlen(date) != 10) {
        return 0;
    }

    // Verify the positions of '-' characters
    if (date[4] != '-' || date[7] != '-') {
        return 0;
    }

    // Verify the positions of digits
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            continue; // Skip '-' characters
        }
        if (date[i] < '0' || date[i] > '9') {
            return 0; // Non-digit characters
        }
    }

    return 1;
}

int validateCommand(char *buffer){
     if (buffer == NULL || (strlen(buffer) == 1 && buffer[0] == '\n')) {
        return 0; 
    }

    int len = strlen(buffer);

    if (buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    char *args[10];
    char *token;
    int argIndex = 0;
    int MAX_ARGS=10;

    char bufferCopy[1024];
    strcpy(bufferCopy, buffer);

    token = strtok(bufferCopy, " \n"); 
    while (token != NULL && argIndex < MAX_ARGS - 1) {
        args[argIndex] = token;
        argIndex++;
        token = strtok(NULL, " \n");
    }
    args[argIndex] = NULL; 

    int numArguments=argIndex;


    if(strcmp(args[0], "dirlist")==0){
        if(numArguments==2 && (strcmp(args[1],"-t")==0 || strcmp(args[1],"-a")==0)){
            return 1;
        }else{
            return 0;
        }
    }else if (strcmp(args[0], "w24fn") == 0) {
        if (numArguments == 2) {
            return 1;
        } else {
            return 0;
        }
    } else if (strcmp(args[0], "w24fz") == 0) {
        if (numArguments == 3) {
            int size1 = atoi(args[1]);
            int size2 = atoi(args[2]);
            if (size1 >= 0 && size2 >= 0 && size1 <= size2) {
                return 1;
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    } else if (strcmp(args[0], "w24ft") == 0) {
        if (numArguments >= 2 && numArguments <= 4) {
            return 1; 
        } else {
            return 0;
        }
    } else if ((strcmp(args[0], "w24fdb") == 0) || strcmp(args[0], "w24fda")==0 ) {
        if (numArguments == 2) {
            if (isValidDateFormat(args[1])==1) {
                return 1;
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    } else if(strcmp(args[0],"file")==0){ // TODO: REMOVE 
        return 1;
    } else if(strcmp(args[0],"hi")==0){
        return 1;
    }
    else{
        return 0;
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
        }

        if(validateCommand(buffer)!=1){
            printf("Invalid command\n\n");
            continue;
        }

        if(strncmp(buffer,"dirlist -a",strlen("dirlist -a"))==0){
            processCommand(sockfd, buffer);
        }else if(strncmp(buffer,"dirlist -t",strlen("dirlist -t"))==0){
            processCommand(sockfd, buffer);           
        }else if(strncmp(buffer,"w24fn",strlen("w24fn"))==0){
            processCommand(sockfd, buffer);
        }else if(strncmp(buffer,"w24fz",strlen("w24fz"))==0){
            processFileCommand(sockfd, buffer);
        }else if(strncmp(buffer,"w24ft",strlen("w24ft"))==0){
            processFileCommand(sockfd, buffer);
        }else if(strncmp(buffer,"w24fdb",strlen("w24fdb"))==0){
            processFileCommand(sockfd, buffer);
        }else if(strncmp(buffer,"w24fda",strlen("w24fda"))==0){
            processFileCommand(sockfd, buffer);
        }else if(strncmp(buffer,"hi",strlen("hi"))==0){
            processFileCommand(sockfd,buffer);
        }else if(strncmp(buffer,"file",strlen("file"))==0){
            processFileCommand(sockfd, buffer);
        }
        else{
            printf("Invalid command, please try again\n");
        }

        printf("\n");
    }
    close(sockfd);
    return 0;
}