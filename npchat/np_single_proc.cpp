#include<iostream>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<map>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<netinet/in.h>
#include"glob.h"

using namespace std;

int main(int argc, char* argv[])  {
    
    // Set port number
    int port = stoi(argv[1]);
    
    // Set variable
    struct sockaddr_in server, client;
    int serverFd, clientFd;

    // Create an AF_INET stream socket to receive 
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Error when create socket
    if(serverFd < 0)   {                    
        fprintf(stderr, "Server - socket() error\n");
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
        close(serverFd);
        exit(-1);
    }
            
    // Set the listen backlog
    if(listen(serverFd, 40)) {
        fprintf(stderr, "Server - listen() error\n");
        close(serverFd);
        exit(-1);
    }
    
    // Set select variable
    int maxfd = serverFd;
    int maxi = -1;
    int nready;
    int id2fd[FD_SETSIZE];
    vector<userPipe*> userPipe_list;
    map<int, int> fd2id;
    map<int, userVar*> id2var;
    map<int, userInfo*> id2info;
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    fd_set allset, rset;
    
    for(int i = 0; i < FD_SETSIZE; i++) {
        id2fd[i] = -1;
    }
    FD_ZERO(&allset);
    FD_SET(serverFd, &allset);

    // Select receive request
    while(1)    {
        rset = allset;                                          // fd set for read
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        
        // Select error
        if(nready < 0)  {
            fprintf(stderr, "Server - select() error\n");
        }
        // Select timeout
        else if(nready == 0)    {
            char buf[100] = "Select timeout\n";
            fprintf(stdout, buf);
            continue;
        }
        else    {
            
            // Service all sockets
            for(int fd = 0; fd <= maxfd; fd++) {
                if(FD_ISSET(fd, &rset))  {
                    
                    // Connection request on original socket
                    if(fd == serverFd)   {

                        // Wait for the incoming connection
                        socklen_t len = (socklen_t)sizeof(client);
                        if((clientFd = accept(serverFd, (struct sockaddr*)&client, &len)) < 0)  {
                            fprintf(stderr, "Server - accept() error\n");
                            close(serverFd);
                            close(clientFd);
                            exit(-1);
                        }

                        // Init client info/var
                        initClient(client, clientFd, id2fd, fd2id, id2info, id2var);
                    
                        // Update allset
                        FD_SET(clientFd, &allset);

                        if(clientFd > maxfd)
                            maxfd = clientFd;
                    
                        // Print Welcome process
                        welcome(clientFd, fd2id, id2info);
                    }

                    // Data arriving fron an already-connected socket
                    else    {
                        
                        // Save ID
                        int id = fd2id[fd];

                        // Receive data
                        char *buf = (char *)calloc(15000, sizeof(char));
                        int recv_len = read(fd, buf, 15000);
                        
                        // Client disconnect or error
                        if(recv_len <= 0)  {
                            
                            // Clear log
                            FD_CLR(fd, &allset);
                            bye(fd, fd2id, id2info);
                            cleanClient(fd, id2fd, fd2id, id2info, id2var, userPipe_list);

                            close(fd);
                            
                            // Message
                            if(recv_len < 0)    {
                                fprintf(stderr, "Server - read() error\n");
                                exit(-1);
                            } 
                            else
                                fprintf(stdout, "Client disconnect.");
                        }

                        // Read data successfully
                        else    {
                            buf = strtok(buf, "\r");
                            buf = strtok(buf, "\n");
                            restoreEnv(id2var[id]);
                            
                            int ret = npshell(buf, fd, id2var[id]->envVar, id2var[id]->numPipe_list, userPipe_list, id2fd, fd2id, id2info);
                            free(buf);

                            // If user type "exit"
                            if(ret == 1)    {                                                              
                                // Clear log
                                FD_CLR(fd, &allset);
                                bye(fd, fd2id, id2info);
                                cleanClient(fd, id2fd, fd2id, id2info, id2var, userPipe_list);

                                shutdown(fd, SHUT_RDWR);
                                close(fd);
                            }
                        }
                    }
                }
            }
        }
    }

    // Close socket
    close(serverFd);

    return 0;
}
