#ifndef NPSHELL
#define NPSHELL

#include<vector>

using namespace std;

class numPipe   {

public: 
    int num;
    int *fd;
    vector<pid_t> numP_pid;
};

void npshell(int clientFd);

#endif
