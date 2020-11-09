#ifndef BUILD_IN
#define BUILD_IN

#include<vector>

using namespace std;

class numPipe   {

public: 
    int num;
    int *fd;
    vector<pid_t> numP_pid;
};

#endif
