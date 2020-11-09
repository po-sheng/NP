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
#include"npshell.h"
#include<algorithm>
#include<sys/stat.h>
#include<dirent.h>
#include<math.h>

using namespace std;

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

// Return the number of number pipe
int numPipe2int(char *cmd)   {
    char *c = (char*)calloc(256, sizeof(char));
    strcpy(c, cmd);

    c = c + 1;
    return stoi(c);
}

// Set the environment variable
void setenv(char **args)   {
                    
    char *var = (char*)calloc(520, sizeof(char)), *value = (char*)calloc(1000, sizeof(char));

    // Copy the env variable and value to a new char*
    strcpy(var, args[1]);       
    strcpy(value, args[2]);

    // Concatenate to produce "putenv" string
    strcat(var, "=");
    strcat(var, value);

    // Set env variable
    putenv(var);

    return;
}

// Print out the environment variable
void printenv(char **args)  {
                    
    // Print value of the given variable
    if(getenv(args[1]) != NULL)
        cout << getenv(args[1]) << endl;
    else
        cout << "";

    return;
}

int main(void)  {

    vector<numPipe*> numPipe_list;

    // Set initial env variable
    setenv("PATH", "bin:.", 1);
    double next_prompt = 0;

    while(1) {
        cout << "% ";                                           // print prompt
//         cout <<"["<<next_prompt<<"] ";
//--------------------------------------------------------------------------------------------------------        
// Read all the input after prompt
        vector<char*> cmd_list;
        char *input = (char*)calloc(15000, sizeof(char));       // use "getchar" to read all the input first
        char *cmd;

        /*                                                      // maybe another way to read input
        cin.getline(input, 15000, "\n");
        if(feof(stdin)) {
            free(input);
            break;
        }
        */

        char c;
        int idx = 0;
        while(1)   {                                            // read character one by one
            if((c = getchar()) == EOF)  {                       // terminate if meet EOF
                free(input);
                return 0;
            }
            if(c == '\n')                                       // stop reading if meet newline
                break;
            input[idx] = c;
            idx++;
        }

        // Continue if it is an empty line with only '\n'
        if(strlen(input) == 0)  {
            continue;
        }
//--------------------------------------------------------------------------------------------------------
// Tokenize the input to commands, symbols and argument by space
        cmd = strtok(input, " ");                               // tokenize the input by whitespace
        while(cmd != NULL)    {                                 // and save it into a command vector
            
            char *temp = (char*)calloc(256, sizeof(char));      // create a copy for command
            strcpy(temp, cmd);
            
            cmd_list.push_back(temp);
            cmd = strtok(NULL, " ");
        }
        free(input);                                            // free the large input space

//---------------------------------------------------------------------------------------------------------
// Set the variable for each line
        string buildIn[3] = {"exit", "setenv", "printenv"};
        vector<int*> pipe_list;                                                     // vector to store all the pipes
        int read_pipe_idx = -1;                                                     // to inform which pipe should the process read
        int r_num_pipe = -1;                                                        // to inform the index of number pipe to read
        int w_num_pipe = -1;                                                        // to inform the index of number pipe to write
        int proc_count = 0, max_proc = 500;                                         // maximum process number will be set to 510
        vector<pid_t> pid_list;

//--------------------------------------------------------------------------------------------------------  
// Implement each command
        for(int idx = 0; idx < cmd_list.size(); idx++)    {
            
            char *head = cmd_list[idx];                                             // declare of the commmand and argument
            char **args;                                                            // arguments include the command
            int counter = 0;                                                        // counter for #arguments 
            args = (char**)calloc(256, sizeof(char*));                              // arguments for command (include head)
            bool from_pipe = false;                                                 // determine whether the stdin of current command is from pipe
            bool from_numPipe = false;                                              // whether the stdin of first command in line is from numpipe
            bool to_numPipe = false;                                                // whether the stdout is pipe to next n command
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
                
                        // Read previous number pipe pid
                        for(int i = 0; i < numPipe_list[idx]->numP_pid.size(); i++) {
                            pid_list.push_back(numPipe_list[idx]->numP_pid[i]);
                            proc_count++;
                        }
                    }
                }
            }

            // Read all the argument until "|", ">" or number pipe
            while(idx < cmd_list.size() && strcmp(cmd_list[idx], "|") != 0 && strcmp(cmd_list[idx], ">") != 0 && !is_numPipe(cmd_list[idx])) { 
                args[counter] = cmd_list[idx];
                counter++;
                idx++;
            }
            
            // Distinguish build-in function
            if(find(begin(buildIn), end(buildIn), head) != end(buildIn))  {     
                if(strcmp(head, "exit") == 0)  {
                    return 0;
                }
                else if(strcmp(head, "setenv") == 0) {
                    setenv(args);
                }
                else if(strcmp(head, "printenv") == 0)   {
                    printenv(args);
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
                        fprintf(stderr, "Pipe create error, error: %d\n", errno);
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
                            fprintf(stderr, "Number pipe create error, error: %d\n", errno);
                            sleep(1000);
                        }
                        numPipe_list.push_back(np);                                     // push number pipe to vector
                        w_num_pipe = numPipe_list.size() - 1;
                    }
                    idx++;                                                              // proceed to end current line       
                }
                
//-------------------------------------------------------------------------------------------------
// Start fork
                // Fork a new process
                proc_count++;
                pid_t pid;                                                     
                
                // Error process
                while((pid = fork()) < 0) {                                             // fork faied
                    fprintf(stderr, "Can not fork, error: %d\n", errno);
                    wait(NULL);
                    proc_count--;
                    usleep(1000);
                }

                // Child process
                if(pid == 0)   {                                                   
                    
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
                    
                    // Start execute
                    execvp(head, args);                                                 // execute the function

                    // If not found, print error message
                    fprintf(stderr, "Unknown command: [%s].\n", head);                  
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

                    // Pipe is own by parent and child, we have to close write end here
                    if(idx < cmd_list.size() && strcmp(cmd_list[idx], "|") == 0)    {
                        int *fd = pipe_list[read_pipe_idx + 1];                         

                        close(fd[1]);
                    }
                    
                    // Retrieve all the dead process
                    if(to_numPipe)  {
                        numPipe_list[w_num_pipe]->numP_pid.push_back(pid_list[pid_list.size()-1]);
                        pipe_list.pop_back();
                        proc_count--;
                    }
                    
                    if(proc_count >= max_proc || (idx >= cmd_list.size() && proc_count > 0))  {
                        int status;
                        int total = pid_list.size();
                        
                        for(int i = 0; i < total; i++)    { 
                            waitpid(pid_list[0], &status, 0);
                            pid_list.erase(pid_list.begin());
                            proc_count--;
                            if(idx >= cmd_list.size() && i == total-1)  {
                                next_prompt = sqrt(WEXITSTATUS(status));
                            }
                        }
                    }
//                     else if(proc_count >= max_proc) {
//                         while(proc_count > 150) {
//                             wait(NULL);
//                             proc_count--;
//                         }
//                     }
                }
            }
        }
//-------------------------------------------------------------------------------------------------------
    }
return 0;
}
