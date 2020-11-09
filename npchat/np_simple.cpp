#include<iostream>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include"npshell.h"

using namespace std;

int main(int argc, char* argv[])  {
    
    // Set port number
    int port = stoi(argv[1]);

    // Set variable
    struct sockaddr_in server, client;
    int serverFd, clientFd;
    char *buf = (char*)calloc(15000, sizeof(char));

    // Create an AF_INET stream socket to receive 
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    
    // error when create socket
    if(serverFd < 0)   {                    
        fprintf(stderr, "Server - socket() error");
        exit(-1);
    }
    
    // Set socket fd reuse
    int reuse = 1;
    if(setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)    {
        fprintf(stderr, "Server - setsockopt() error\n");
    }

    // Set bind
    memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
        
    // Bind the socket
    if(bind(serverFd, (struct sockaddr *)&server, sizeof(server)) < 0)  {
        fprintf(stderr, "Server - bind() error\n");
        shutdown(serverFd, SHUT_RDWR);
        close(serverFd);
        free(buf);
        exit(-1);
    }
            
    // Set the listen backlog
    if(listen(serverFd, 5)) {
        fprintf(stderr, "Server - listen() error\n");
        shutdown(serverFd, SHUT_RDWR);
        close(serverFd);
        free(buf);
        exit(-1);
    }
    while(1)    {            
        
        // Wait for the incoming connection
        socklen_t len = (socklen_t)sizeof(client);
        if((clientFd = accept(serverFd, (struct sockaddr*)&client, &len)) < 0)  {
            fprintf(stderr, "Server - accept() error\n");
            shutdown(clientFd, SHUT_RDWR);
            close(clientFd);
            shutdown(serverFd, SHUT_RDWR);
            close(serverFd);
            free(buf);
            exit(-1);
        }
    
        // Client info
        char *client_ip = inet_ntoa(client.sin_addr);
        int client_port = ntohs(client.sin_port);

        // Process npshell
        npshell(clientFd);
   
        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }
    
    // Close socket
    shutdown(serverFd, SHUT_RDWR);
    close(serverFd);
    free(buf);

    return 0;
}
