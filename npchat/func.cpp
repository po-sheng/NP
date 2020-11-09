#include<iostream>
#include<string.h>
#include<string>
#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include"glob.h"
#include<arpa/inet.h>
#include<unistd.h>

using namespace std;

char* int2char(int num)    {
    char *buf = (char *)calloc(100, sizeof(char));
    sprintf(buf, "%d", num);

    return buf;
}

void cleanClient(int fd, int *id2fd, map<int, int> &fd2id, map<int, userInfo*> &id2info, map<int, userVar*> &id2var, vector<userPipe*> &userPipe_list)    {
    int id = fd2id[fd];
    
    id2fd[id] = -1;

    fd2id.erase(fd);
    
    // Clean user info
    userInfo *info;
    info = id2info[id];
    id2info.erase(id);
    delete info->name;
    delete info;
    
    // Clean user var
    userVar *var;
    var = id2var[id];
    id2var.erase(id);
    delete var;
    
    // Clean user pipe
    int devNull = open("/dev/null", O_RDWR); 
    for(int i = 0; i < userPipe_list.size(); i++)   {
        if(userPipe_list[i]->src_id == id || userPipe_list[i]->dst_id == id)  {
            
            // pipe user pipe to /dev/null
            int *fd = (int*)calloc(2, sizeof(int));                         
            char *buf = (char*)calloc(10000, sizeof(char)); 
            fd = userPipe_list[i]->fd;
            
            close(fd[1]);
            int count = read(fd[0], buf, 10000);
            while(count != 0)   {
                write(devNull, buf, strlen(buf));
                count = read(fd[0], buf, 10000);
            }
            close(fd[0]);
           
            // Retrieve dead process
            waitpid(userPipe_list[i]->userP_pid, NULL, 0);

            // remove user pipe in userPipe_list
            userPipe_list.erase(userPipe_list.begin() + i);
        
            free(fd);
            free(buf);
        }
    }

    return;
}

void initClient(struct sockaddr_in client, int clientFd, int *id2fd, map<int, int> &fd2id, map<int, userInfo*> &id2info, map<int, userVar*> &id2var)   {
    
    // Build id2fd and fd2id
    int id = FD_SETSIZE;
    while(id == FD_SETSIZE) {
        for(id = 0; id < FD_SETSIZE; id++) {
            if(id2fd[id] < 0)    {
                id2fd[id] = clientFd;
                fd2id[clientFd] = id;
                break;
            }
        }
        usleep(1000);
    }
    
    // Build id2info
    userInfo *info = new userInfo;
    info->name = (char *)calloc(300, sizeof(char));
    
    strcpy(info->name, "(no name)");
    info->IP = inet_ntoa(client.sin_addr);
    info->port = int2char(ntohs(client.sin_port));

    id2info[id] = info;

    // Build id2var
    userVar *var = new userVar;
    char *env = (char *)calloc(300, sizeof(char));
    char *val = (char *)calloc(300, sizeof(char));

    strcpy(env, "PATH");
    strcpy(val, "bin:.");
    var->envVar[env] = val;

    id2var[id] = var;

    return;
}

void restoreEnv(userVar *var)   {
    // Clear all the environment variables
    while(clearenv() != 0)  {
        usleep(1000);
    }
    
    // Set environment variable
    map<char*, char*>::iterator it;
    map<char*, char*>::iterator begin = var->envVar.begin();
    map<char*, char*>::iterator end = var->envVar.end();
    int is_init = 1;
    for(it = begin; it != end; it++)  {
        setenv(it->first, it->second, var->envVar, is_init);
    }
    
    return;
}

void welcome(int fd, map<int, int> fd2id, map<int, userInfo*> id2info)  {
    
    int id = fd2id[fd];

    // Create messages
    char *title = (char *)calloc(500, sizeof(char));  
    char *messg = (char *)calloc(100, sizeof(char));
    char *pmpt = (char *)calloc(10, sizeof(char));

    // Title
    for(int i = 0; i < 40; i++) 
        strcat(title, "*");
    strcat(title, "\n");
    strcat(title, "** Welcome to the information server. **\n");
    for(int i = 0; i < 40; i++) 
        strcat(title, "*");
    strcat(title, "\n");

    // Message    
    strcpy(messg, "*** User '(no name)' entered from ");
    strcat(messg, id2info[id]->IP);
    strcat(messg, ":");
    strcat(messg, id2info[id]->port);
    strcat(messg, ". ***\n");
    
    // Prompt
    strcpy(pmpt, "% ");

    // Write welcome message to current client
    write(fd, title, strlen(title));

    // Write welcome message to all the clietn
    map<int, int>::iterator it;
    map<int, int>::iterator begin = fd2id.begin();
    map<int, int>::iterator end = fd2id.end();
    for(it = begin; it != end; it++)  {
        write(it->first, messg, strlen(messg));
    }
    
    // Write prompt to current client
    write(fd, pmpt, strlen(pmpt));

    free(title);
    free(messg);
    free(pmpt);

    return;
}

void bye(int fd, map<int, int> fd2id, map<int, userInfo*> id2info)    {
    char *messg = (char *)calloc(100, sizeof(char));

    int id = fd2id[fd];

    // Create goodbye message
    strcat(messg, "*** User '");
    strcat(messg, id2info[id]->name);
    strcat(messg, "' left. ***\n");

    // Write to all the other client
    map<int, int>::iterator it;
    for(it = fd2id.begin(); it != fd2id.end(); it++)    {
        if(it->first != fd) 
            write(it->first, messg, strlen(messg));
    }

    free(messg);

    return;
}

void missUser(int fd, char *id) {
    char *err = (char *)calloc(100, sizeof(char));
    strcat(err, "*** Error: user #");
    strcat(err, id);
    strcat(err, " does not exist yet. ***\n");

    write(fd, err, strlen(err));
        
    free(err);
    
    return;
}

void who(int fd, int *id2fd, map<int, int> fd2id, map<int, userInfo*> id2info)  {
    int id = fd2id[fd];
    char *title = (char *)calloc(200, sizeof(char));
    
    // Write title
    strcat(title, "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
    write(fd, title, strlen(title));

    for(int i = 0; i < FD_SETSIZE; i++)   {
        if(id2fd[i] != -1)  {
            char *messg = (char *)calloc(200, sizeof(char));
        
            // Create data
            strcat(messg, int2char(i + 1));
            strcat(messg, "\t");
            strcat(messg, id2info[i]->name);
            strcat(messg, "\t");
            strcat(messg, id2info[i]->IP);
            strcat(messg, ":");
            strcat(messg, id2info[i]->port);
            if(fd == id2fd[i]) 
                strcat(messg, "\t<-me");
            strcat(messg, "\n");

            // Write data
            write(fd, messg, strlen(messg));

            free(messg);
        }
    }

    return;
}

void tell(int fd, int *id2fd, map<int, int> fd2id, map<int, userInfo*> id2info, char **args) {
    int id = fd2id[fd];

    int dest_id = stoi(args[1]);

    // Broadcast to other user
    char *buf = (char *)calloc(1000, sizeof(char));

    // Create message
    strcat(buf, "*** ");
    strcat(buf, id2info[id]->name);
    strcat(buf, " told you ***: ");
    
    int idx = 2;
    while(args[idx] != NULL)   {
        strcat(buf, args[idx]);
        strcat(buf, " ");
        idx++;
    }
    strcat(buf, "\n");
    
    // Check if user exist
    if(id2fd[dest_id - 1] == -1)    {   
        missUser(fd, args[1]);
    }
    else    {
        write(id2fd[dest_id - 1], buf, strlen(buf));
    }

    free(buf);

    return;
}

void yell(int fd, map<int, int> fd2id, map<int, userInfo*> id2info, char **args) {
    int id = fd2id[fd];

    // Broadcast to other user
    map<int, int>::iterator it;
    char *buf = (char *)calloc(1000, sizeof(char));

    // Create message
    strcat(buf, "*** ");
    strcat(buf, id2info[id]->name);
    strcat(buf, " yelled ***: ");
    
    int idx = 1;
    while(args[idx] != NULL)   {
        strcat(buf, args[idx]);
        strcat(buf, " ");
        idx++;
    }
    strcat(buf, "\n");

    // Write to all the client
    for(it = fd2id.begin(); it != fd2id.end(); it++)    {
        write(it->first, buf, strlen(buf));
    }

    free(buf);

    return;
}

void name(int fd, map<int, int> fd2id, map<int, userInfo*> &id2info, char **args) {
    int id = fd2id[fd];
 
    // Check repeat
    map<int, userInfo*>::iterator info_it;
    for(info_it = id2info.begin(); info_it != id2info.end(); info_it++)  {
        if(info_it->first != id && strcmp(args[1], info_it->second->name) == 0)    
            break;
    }

    // Broadcast to other user
    map<int, int>::iterator it;
    char *buf = (char *)calloc(100, sizeof(char));
    
    if(info_it == id2info.end())    { 
        // Set new name
        id2info[id]->name = args[1];
        
        // Create message
        strcat(buf, "*** User from ");
        strcat(buf, id2info[id]->IP);
        strcat(buf, ":");
        strcat(buf, id2info[id]->port);
        strcat(buf, " is named '");
        strcat(buf, args[1]);
        strcat(buf, "'. ***\n");
        
        // Write to all the client
        for(it = fd2id.begin(); it != fd2id.end(); it++)    {
            write(it->first, buf, strlen(buf));
        }
    }
    else    {
        // Create message
        strcat(buf, "*** User '");
        strcat(buf, args[1]);
        strcat(buf, "' already exists. ***\n");
    
        // Write to current client
        write(fd, buf, strlen(buf));
    }

    free(buf);

    return;
}

// Write message for repeat user pipe
void repeat_userPipe(int fd, int src_id, int dst_id)    {

    // Create error message
    char *err = (char *)calloc(100, sizeof(err));

    strcat(err, "*** Error: the pipe #");
    strcat(err, int2char(src_id));
    strcat(err, "->#");
    strcat(err, int2char(dst_id));
    strcat(err, " already exists. ***\n");
    
    // Write to client
    write(fd, err, strlen(err));

    free(err);
    return;
}

// Write message for missing user pipe 
void miss_userPipe(int fd, int src_id, int dst_id)  {
    
    // Create error message
    char *err = (char *)calloc(100, sizeof(err));
    
    strcat(err, "*** Error: the pipe #");
    strcat(err, int2char(src_id));
    strcat(err, "->#");
    strcat(err, int2char(dst_id));
    strcat(err, " does not exist yet. ***\n");

    // Write to client
    write(fd, err, strlen(err));

    free(err);
    return;
}

// Write message for writing user pipe and check if error happened
int to_userP_val(int fd, int src_id, int dst_id, vector<userPipe*> userPipe_list, char *cmd, int *id2fd, map<int, int> fd2id, map<int, userInfo*> id2info)   {

    // Check if destination id exist
    if(id2fd[dst_id] == -1) {
        missUser(fd, int2char(dst_id + 1));                                  // destination client not exist
        return 1;
    }   

    // Check if user pipe exist
    int idx;
    for(idx = 0; idx < userPipe_list.size(); idx++) {
        
        // User pipe exist
        if(userPipe_list[idx]->src_id == src_id && userPipe_list[idx]->dst_id == dst_id)    {
            break;    
        }
    }

    // Error when user pipe exist
    if(idx != userPipe_list.size()) {          
        repeat_userPipe(fd, src_id + 1, dst_id + 1);
        return 1;
    }
    
    // Write broadcast message 
    char *buf = (char *)calloc(200, sizeof(char));   
    map<int, int>::iterator it;

    strcat(buf, "*** ");
    strcat(buf, id2info[src_id]->name);
    strcat(buf, " (#");
    strcat(buf, int2char(src_id + 1));
    strcat(buf, ") just piped '");
    strcat(buf, cmd);
    strcat(buf, "' to ");
    strcat(buf, id2info[dst_id]->name);
    strcat(buf, " (#");
    strcat(buf, int2char(dst_id + 1));
    strcat(buf, ") ***\n");

    // Write to all the client
    for(it = fd2id.begin(); it != fd2id.end(); it++)    {
        write(it->first, buf, strlen(buf));
    }

    free(buf);
    
    return 0;
}

// Write message for reading user pipe and check if error happened
int from_userP_val(int fd, int src_id, int dst_id, vector<userPipe*> userPipe_list, char *cmd, int *id2fd, map<int, int> fd2id, map<int, userInfo*> id2info, int &userP_idx) {
    
    // Check if source id exist
    if(id2fd[src_id] == -1) {
        missUser(fd, int2char(src_id + 1));                                  // source client not exist
        return 1;
    }   
    
    // Check if user pipe exist
    int idx;
    for(idx = 0; idx < userPipe_list.size(); idx++) {
        
        // User pipe exist
        if(userPipe_list[idx]->src_id == src_id && userPipe_list[idx]->dst_id == dst_id)    {
            userP_idx = idx;
            break;    
        }
    }

    // Error when user pipe not exist
    if(idx == userPipe_list.size()) {                 
        miss_userPipe(fd, src_id + 1, dst_id + 1);
        return 1;
    }
    
    // Create broadcast message
    char *buf = (char *)calloc(200, sizeof(char));   
    map<int, int>::iterator it;

    strcat(buf, "*** ");
    strcat(buf, id2info[dst_id]->name);
    strcat(buf, " (#");
    strcat(buf, int2char(dst_id + 1));
    strcat(buf, ") just received from ");
    strcat(buf, id2info[src_id]->name);
    strcat(buf, " (#");
    strcat(buf, int2char(src_id + 1));
    strcat(buf, ") by '");
    strcat(buf, cmd);
    strcat(buf, "' ***\n");

    // Write to all the client
    for(it = fd2id.begin(); it != fd2id.end(); it++)    {
        write(it->first, buf, strlen(buf));
    }

    free(buf);
    return 0;
}

