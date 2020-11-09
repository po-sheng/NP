#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include<string.h>
#include<vector>
#include<errno.h>
#include<sys/types.h>
#include<fcntl.h>
#include<algorithm>
#include<sys/stat.h>
#include<dirent.h>
#include<math.h>
#include"glob.h"

using namespace std;

// Return true if the input char* belongs to user pipe
bool is_userPipe(char *cmd)  {
    
    char *c = (char*)calloc(256, sizeof(char));
    strcpy(c, cmd);                                              // not affect the original command
    
    if(strlen(c) >= 2 && (c[0] == '<' || c[0] == '>'))  {
        c = c + 1;
        for(int i = 0; i < strlen(c); i++)  {
            if(c[i] < 48 || c[i] > 57)
                return false;
        }
        return true;
    }
    return false;
}

// Return true if the input char* belongs to number pipe
bool is_numPipe(char *cmd)   {
    char *c = (char*)calloc(256, sizeof(char));
    strcpy(c, cmd);                                              // not affect the original command
    
    if(strlen(c) >= 2 && (c[0] == '|' || c[0] == '!'))  {
        c = c + 1;
        for(int i = 0; i < strlen(c); i++)  {
            if(c[i] < 48 || c[i] > 57)
                return false;
        }
        return true;
    }
    return false;
}

// Return the number of number pipe/user pipe
int numPipe2int(char *cmd)   {
    char *c = (char*)calloc(256, sizeof(char));
    strcpy(c, cmd);

    c = c + 1;
    return stoi(c);
}

// Set the environment variable
void setenv(char *var, char *val, map<char*, char*> &envVar, int is_init)   {
                    
    char *args = (char*)calloc(500, sizeof(char));

    // Store in envVar
    if(is_init == 0)    {
        envVar[var] = val;
    }

    // Concatenate to produce "putenv" string
    strcat(args, var);
    strcat(args, "=");
    strcat(args, val);

    // Set env variable
    putenv(args);

    return;
}

// Print out the environment variable
void printenv(char **args, int fd)  {
                    
    // Print value of the given variable
    if(getenv(args[1]) != NULL) {
        char *buf = (char *)calloc(100, sizeof(char));   
        strcpy(buf, getenv(args[1]));
        strcat(buf, "\n");
        write(fd, buf, strlen(buf));
    }

    return;
}

int npshell(char *input, int clientFd, map<char*, char*> &envVar, vector<numPipe*> &numPipe_list, vector<userPipe*> &userPipe_list, int *id2fd, map<int, int> fd2id, map<int, userInfo*> id2info)   {
    
    // Continue if it is an empty line with only '\n'
    if(strlen(input) == 0)  {
        write(clientFd, "% ", 2*sizeof(char));                  // print prompt
        return 0;
    }
    
//--------------------------------------------------------------------------------------------------------        
// Set variable
    char *buf = (char *)calloc(15000, sizeof(char));
    strcpy(buf, input);
    vector<char*> cmd_list;
    char *cmd;
    
//--------------------------------------------------------------------------------------------------------
// Tokenize the input to commands, symbols and argument by space
    cmd = strtok(buf, " ");                               // tokenize the input by whitespace
    while(cmd != NULL)    {                                 // and save it into a command vector
            
        char *temp = (char*)calloc(256, sizeof(char));      // create a copy for command
        strcpy(temp, cmd);
            
        cmd_list.push_back(temp);
        cmd = strtok(NULL, " ");
    }
    free(buf);                                            // free the large input space

//---------------------------------------------------------------------------------------------------------
// Set the variable for each line
    string buildIn[7] = {"exit", "setenv", "printenv", "who", "tell", "yell", "name"};
    vector<int*> pipe_list;                                                     // vector to store all the pipes
    int read_pipe_idx = -1;                                                     // to inform which pipe should the process read
    int r_num_pipe = -1;                                                        // to inform the index of number pipe to read
    int w_num_pipe = -1;                                                        // to inform the index of number pipe to write
    int r_user_pipe = -1;                                                       // to inform the index of user pipe to read
    int w_user_pipe = -1;                                                       // to inform the index of user pipe to write
    int proc_count = 0, max_proc = 16;                                         // maximum process number will be set to max_proc
    vector<pid_t> pid_list;

//--------------------------------------------------------------------------------------------------------  
// Implement each command
    for(int idx = 0; idx < cmd_list.size(); idx++)    {
            
        char *head = cmd_list[idx];                                             // declare of the commmand and argument
        char **args;                                                            // arguments include the command
        args = (char**)calloc(256, sizeof(char*));                              // arguments for command (include head)
        int counter = 0;                                                        // counter for #arguments 
        int to_numP_id = -1, from_numP_id = -1;                                 // record the id of user pipe 
        int to_ret = 0, from_ret = 0;                                           // show if current user pipe is legal
        int from_userP_id = 0, to_userP_id = 0;                                 // represent the user id of user pipe

        bool from_pipe = false;                                                 // determine whether the stdin of current command is from pipe
        bool from_numPipe = false;                                              // whether the stdin of first command in line is from numpipe
        bool from_userPipe = false;                                             // whether the stdin is pipe from other user
        bool to_numPipe = false;                                                // whether the stdout is pipe to next n command
        bool to_userPipe = false;                                                // whether the stdout is pipe to other user
        bool to_errPipe = false;                                                // whether the stderr is pipe to next n command
            
        // Whether the stdin of current command is from pipe
        if(idx != 0 && strcmp(cmd_list[idx - 1], "|") == 0) { 
            from_pipe = true;
        }

        // whether there is a stdout/stderr pipe to first command
        if(idx == 0)    {
            for(int idx = 0; idx < numPipe_list.size(); idx++)    {
                numPipe_list[idx]->num--;
                if(numPipe_list[idx]->num == 0)    {
                    close(numPipe_list[idx]->fd[1]);
                    r_num_pipe = idx;
                    from_numPipe = true;
                    
                    // Restore number pipe pid
                    for(int i = 0; i < numPipe_list[idx]->numP_pid.size(); i++)   {
                        pid_list.push_back(numPipe_list[idx]->numP_pid[i]);
                        proc_count++; 
                    }
                }
            }
        }

        // Read all the argument until "|", ">" or number pipe
        while(idx < cmd_list.size() && ((find(begin(buildIn), end(buildIn), head) != end(buildIn)) || (strcmp(cmd_list[idx], "|") != 0 && strcmp(cmd_list[idx], ">") != 0 && !is_numPipe(cmd_list[idx])))) { 
            
            // If user pipe are in the middle of command
            if(is_userPipe(cmd_list[idx])) {
                if(cmd_list[idx][0] == '<') {
                    from_userPipe = true;
                    from_userP_id = numPipe2int(cmd_list[idx]) - 1;                 // the real id count from 0
                }
                else    {
                    to_userPipe = true;
                    to_userP_id = numPipe2int(cmd_list[idx]) - 1;                   // the real id count from 0
                }
            }
            else    {
                args[counter] = cmd_list[idx];
                counter++;
            }
            
            idx++;
        }
        
        // Distinguish build-in function
        if(find(begin(buildIn), end(buildIn), head) != end(buildIn))  {     
            if(strcmp(head, "exit") == 0)  {
                return 1;
            }
            else if(strcmp(head, "setenv") == 0) {
                int is_init = 0;
                setenv(args[1], args[2], envVar, is_init);
            }
            else if(strcmp(head, "printenv") == 0)   {
                printenv(args, clientFd);
            }
            else if(strcmp(head, "who") == 0)   {
                who(clientFd, id2fd, fd2id, id2info); 
            }
            else if(strcmp(head, "tell") == 0)   {
                tell(clientFd, id2fd, fd2id, id2info, args); 
            }
            else if(strcmp(head, "yell") == 0)   {
                yell(clientFd, fd2id, id2info, args); 
            }
            else if(strcmp(head, "name") == 0)   {
                name(clientFd, fd2id, id2info, args); 
            }
        }

        // Find executable files in /bin
        else    {                                                                      
                
            // Read ">"
            char *file = NULL;                                                      // get the file name
            if(idx < cmd_list.size() && strcmp(cmd_list[idx], ">") == 0)    {
                idx++;
                file = cmd_list[idx];
                while(idx < cmd_list.size() && strcmp(cmd_list[idx], "|") != 0)    {
                    idx++;
                }
            }
                
            // STDIN from pipe
            if(from_pipe)    {
                read_pipe_idx = pipe_list.size() - 1;                               // inform the pipe index of current command 
            }

            // Read "|"
            if(idx < cmd_list.size() && strcmp(cmd_list[idx], "|") == 0)  {
                int *fd = (int*)calloc(2, sizeof(int));                             // create a new pipe       
                    
                while(pipe(fd) < 0)    {                                            // initial the pipe
                    char buf[100] = "Pipe create error\n";
                    write(clientFd, buf, strlen(buf));
                    usleep(1000);
                }
                    
                pipe_list.push_back(fd);                                            // add the new pipe to the back for next command 
            }

            // Read number pipe "|n" or "!n"
            if(idx < cmd_list.size() && is_numPipe(cmd_list[idx]))   {
                    
                to_numPipe = true;
                    
                // If command start with '!', another pipe for stderr
                if(cmd_list[idx][0] == '!') {
                    to_errPipe = true;
                }
                    
                // Find if there exist a pipe with same number 
                for(int i = 0; i < numPipe_list.size(); i++)   {
                    numPipe *np;
                    np = numPipe_list[i];

                    if(np->num == numPipe2int(cmd_list[idx]))    {
                        w_num_pipe = i;
                    }
                }
                    
                // If there don't have a pipe with same number 
                if(w_num_pipe == -1)  {
                    // Pipe for stdout
                    int *fd = (int*)calloc(2, sizeof(int));                         // create a new pipe

                    numPipe *np = new numPipe;                                      // new an object for number pipe
                    np->num = numPipe2int(cmd_list[idx]);                           // set value of number pipe
                    np->fd = fd;

                    while(pipe(fd) < 0) {
                        char buf[100] = "Number pipe create error\n";
                        write(clientFd, buf, strlen(buf));
                        sleep(1000);
                    }
                    numPipe_list.push_back(np);                                     // push number pipe to vector
                    w_num_pipe = numPipe_list.size() - 1;
                }
                idx++;                                                              // proceed to end current line       
            }
           
            // Read from user pipe "<n"
            if(from_userPipe)   {
                
                // Print message and check validation of user pipe
                from_ret = from_userP_val(clientFd, from_userP_id, fd2id[clientFd], userPipe_list, input, id2fd, fd2id, id2info, r_user_pipe);                    
                
                if(from_ret == 0)   {
                    // Close user pipe write end
                    close(userPipe_list[r_user_pipe]->fd[1]);
                    
                    // Restore user pipe pid
                    pid_list.push_back(userPipe_list[r_user_pipe]->userP_pid);

                    // For individual proc_count
                    // proc_count++;    
                }
            }

            // Write to user pipe ">n"
            if(to_userPipe) {
                
                // Print message and check validation of user pipe
                to_ret = to_userP_val(clientFd, fd2id[clientFd], to_userP_id, userPipe_list, input, id2fd, fd2id, id2info);                    
               
                // Pipe for stdout
                int *fd = (int*)calloc(2, sizeof(int));                         // create a new pipe

                while(pipe(fd) < 0) {
                    char buf[100] = "User pipe create error\n";
                    write(clientFd, buf, strlen(buf));
                    sleep(1000);
                }
                
                // record user pipe only when pipe is valid
                if(to_ret == 0) {
                
                    // New userPipe object
                    userPipe *up = new userPipe;                                    // new an object for number pipe
                    up->src_id = fd2id[clientFd];                                   // set value of number pipe
                    up->dst_id = to_userP_id;                                       // set value of number pipe
                    up->fd = fd;

                    userPipe_list.push_back(up);                                    // push number pipe to vector
                }
                
                w_user_pipe = userPipe_list.size() - 1; 
            }

//-------------------------------------------------------------------------------------------------
// Start fork
            // Fork a new process
            proc_count++;
            pid_t pid;                                                     
                
            // Error process
            while((pid = fork()) < 0) {                                             // fork faied
//                 char buf[100] = "Can not fork\n";
//                 write(clientFd, buf, strlen(buf));
//                 wait(NULL);
//                 proc_count--;
                usleep(1000);
            }

            // Child process
            if(pid == 0)   {                                                   
                
                // Open null file for user pipe
                int devNull = open("/dev/null", O_RDWR);
                
                // set output and error to clientFd
                dup2(clientFd, STDOUT_FILENO);
                dup2(clientFd, STDERR_FILENO);
                    
                // ">" to file
                if(file != NULL) {
                    int fp = open(file, O_CREAT|O_WRONLY|O_TRUNC, 0644);            // open file
                    dup2(fp, STDOUT_FILENO);                                        // set the command output to the file
                    close(fp);
                }
                        
                // Read from pipe, input form last pipe
                if(from_pipe)    {
                            
                    int *prev_fd = pipe_list[read_pipe_idx];                        // get the previous pipe from last command
                        
                    // We can only close the pipe end in parant and child of last command
                    dup2(prev_fd[0], STDIN_FILENO);                                 // copy the file discription from STDIN to prev_fd[0]
                }
                // Write to pipe, pipe to next command
                if(idx < cmd_list.size() && strcmp(cmd_list[idx], "|") == 0)    {
                            
                    int *fd = pipe_list[read_pipe_idx + 1];                         // current new pipe for writing
                            
                    close(fd[0]);
                    dup2(fd[1], STDOUT_FILENO);                                     // copy the file discription from STDOUT to fd[1]   
                    close(fd[1]);
                }
                // Read from number pipe, from n lines above
                if(from_numPipe)    {
                            
                    int *prev_fd = numPipe_list[r_num_pipe]->fd;                    // get the previous pipe from last command
                                 
                    dup2(prev_fd[0], STDIN_FILENO);
                }
                // Write to number pipe, pipe to next n lines
                if(to_numPipe)  {
                               
                    int *fd = numPipe_list[w_num_pipe]->fd;                         // current new pipe for writing
                            
                    close(fd[0]);
                    dup2(fd[1], STDOUT_FILENO);                                     // copy the file discription from STDOUT to fd[1]   
                        
                    // Write to error pipe, pipe to next n lines
                    if(to_errPipe)  {       
                        dup2(fd[1], STDERR_FILENO);                                 // copy the file discription from STDOUT to fd[1]   
                    }

                    close(fd[1]);
                }
                // Read from user pipe
                if(from_userPipe)   {
                    
                    // User not exist or user pipe not exist
                    if(from_ret != 0)    {
                        
                        // Read from /dev/null       
                        dup2(devNull, STDIN_FILENO);
                    }
                    // User pipe can be read
                    else    {
                        int *prev_fd = userPipe_list[r_user_pipe]->fd;                    // get the previous pipe from last command
                                 
                        dup2(prev_fd[0], STDIN_FILENO);
                    }
                }

                // Write to user pipe
                if(to_userPipe) {
                    
                    // User not exist
                    if(to_ret != 0)    {
                        
                        // Write to /dev/null
                        dup2(devNull, STDOUT_FILENO);
                    }
                    // User pipe can be write
                    else    {
                        
                        int *fd = userPipe_list[w_user_pipe]->fd;                       // current new pipe for writing
                            
                        close(fd[0]);
                        dup2(fd[1], STDOUT_FILENO);                                     // copy the file discription from STDOUT to fd[1]   

                        close(fd[1]);
                    }
                }
                
                char *temp;
                int counter = 0;
                temp = args[counter];
                while(temp != NULL) {
                    counter++;
                    temp = args[counter];
                }

                // Start execute
                execvp(head, args);                                                     // execute the function

                // If not found, print error message
                char *buf = (char *)calloc(300, sizeof(char));
                strcat(buf, "Unknown command: [");
                strcat(buf, head);
                strcat(buf, "].\n");
                write(clientFd, buf, strlen(buf));                  
                exit(1);
            }

            // Father process
            else    {
                pid_list.push_back(pid);
                    
                if(from_pipe)    {
                    int *prev_fd = pipe_list[read_pipe_idx];                        // get the previous pipe from last command
                    
                    close(prev_fd[0]);
                }
                    
                // Close number pipe read end
                if(from_numPipe)   {
                    int *prev_fd = numPipe_list[r_num_pipe]->fd;                    // get the previous pipe from last command
                    
                    close(prev_fd[0]);
                    numPipe_list.erase(numPipe_list.begin() + r_num_pipe);
                }

                // Close user pipe read end
                if(from_userPipe)   {
                    if(from_ret == 0)    {
                        int *prev_fd = userPipe_list[r_user_pipe]->fd;                    // get the previous pipe from last command
                    
                        close(prev_fd[0]);
                        
                        userPipe_list.erase(userPipe_list.begin() + r_user_pipe);
                    }
                }
                
                // Pipe is own by parent and child, we have to close write end here
                if(idx < cmd_list.size() && strcmp(cmd_list[idx], "|") == 0)    {
                    int *fd = pipe_list[read_pipe_idx + 1];                         

                    close(fd[1]);
                }
                    
                // Retrieve all the dead process
                if(to_numPipe)  {
                    numPipe *np;
                    np = numPipe_list[numPipe_list.size() - 1];

                    np->numP_pid.push_back(pid_list[pid_list.size()-1]);
                    pid_list.pop_back();
                    proc_count--;
                }
                if(to_userPipe && to_ret == 0) {
                    userPipe *up;
                    up = userPipe_list[userPipe_list.size() - 1];

                    up->userP_pid = pid_list[pid_list.size()-1];
                    pid_list.pop_back();
                    proc_count--;
                }
                
                if(proc_count >= max_proc || (idx >= cmd_list.size() && proc_count > 0))  {
                    int status;
                    int total = pid_list.size();
                        
                    for(int i = 0; i < total; i++)    { 
                        waitpid(pid_list[0], &status, 0);
                        pid_list.erase(pid_list.begin());
                        proc_count--;
                    }
                }
            }
        }
    }
    write(clientFd, "% ", 2*sizeof(char));                  // print prompt
//-------------------------------------------------------------------------------------------------------
return 0;
}
