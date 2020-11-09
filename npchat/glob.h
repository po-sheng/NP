#ifndef NP_SIG_PROC
#define NP_SIG_PROC

#include<map>
#include<vector>
#include<netinet/in.h>

using namespace std;

class numPipe   {
public: 
    int num;
    int *fd;
    vector<pid_t> numP_pid;
};

class userPipe   {
public: 
    int src_id;
    int dst_id;
    int *fd;
    pid_t userP_pid;
};

class userVar  {
public:
    map<char*, char*> envVar;
    vector<numPipe *> numPipe_list;
};

class userInfo   {
public:
    char *name;
    char *port;
    char *IP;
};

void setenv(char *var, char *val, map<char*, char*> &envVar, int is_init);
int npshell(char *input, int clientFd, map<char*, char*> &envVar, vector<numPipe*> &numPipe_list, vector<userPipe*> &userPipe_list, int *id2fd, map<int, int> fd2id, map<int, userInfo*> id2info);

void cleanClient(int fd, int *id2fd, map<int, int> &fd2id, map<int, userInfo*> &id2info, map<int, userVar*> &id2var, vector<userPipe*> &userPipe_list);
void initClient(struct sockaddr_in client, int clientFd, int *id2fd, map<int, int> &fd2id, map<int, userInfo*> &id2info, map<int, userVar*> &id2var);
void restoreEnv(userVar *var);
void welcome(int fd, map<int, int> fd2id, map<int, userInfo*> id2info);
void bye(int fd, map<int, int> fd2id, map<int, userInfo*> id2info);
void missUser(int fd, char *id);

void who(int fd, int *id2fd, map<int, int> fd2id, map<int, userInfo*> id2info);
void tell(int fd, int *id2fd, map<int, int> fd2id, map<int, userInfo*> id2info, char **args);
void yell(int fd, map<int, int> fd2id, map<int, userInfo*> id2info, char **args);
void name(int fd, map<int, int> fd2id, map<int, userInfo*> &id2info, char **args);

int to_userP_val(int fd, int src_id, int dst_id, vector<userPipe*> userPipe_list, char *cmd, int *id2fd, map<int, int> fd2id, map<int, userInfo*> id2info);
int from_userP_val(int fd, int src_id, int dst_id, vector<userPipe*> userPipe_list, char *cmd, int *id2fd, map<int, int> fd2id, map<int, userInfo*> id2info, int &userP_idx);

#endif
