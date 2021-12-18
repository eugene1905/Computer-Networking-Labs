#include "common.h"

int routerCnt;
Router routers[MAX_ROUTER];
map<int, int> indexMap;

void ReadFiles(char* loc_path);
int GetCommand(int* cmd, int* src, int* dest, int* cost);
int ConnectRouter(int* sockfd, int i);

int main(int argc, char** argv){
    setvbuf(stdout, NULL, _IONBF, 0); // to accommodate automated tests
    if(argc != 2){
        //printf("usage: ./agent <router location file>\n");
        return 0;
    }
    
    ReadFiles(argv[1]);

    // loop for sending commands
    while(1){
        int connfd;
        int cmd, src, dest, cost;
        int dv[MAX_ROUTER] = {0}, nextHop[MAX_ROUTER] = {0};
        if(GetCommand(&cmd, &src, &dest, &cost) < 0 )
            break;
        if(cmd == RS_DV){
            //printf("Sending DV to all routers\n");
            for(int i = 0; i < routerCnt; i++){
                if(ConnectRouter(&connfd, i) < 0)
                    return -1;
                if(SendMessage(connfd, RS_DV, -1, 0, 0, 0, 0) < 0)
                    return -1;
                close(connfd);
            }
        }
        else if(cmd == RS_UPDATE){
            //printf("Sending UPDATE to router %d\n", src);
            if(ConnectRouter(&connfd, src) < 0)
                return -1;
            if(SendMessage(connfd, RS_UPDATE, -1, dest, cost, 0, 0) < 0)
                return -1;
            close(connfd);
        }
        else if(cmd == RS_SHOW){
            int type;
            //printf("Sending SHOW to router %d\n", src);
            if(ConnectRouter(&connfd, src) < 0)
                return -1;
            if(SendMessage(connfd, RS_SHOW, -1, 0, 0, 0, 0) < 0)
                return -1;
            if(ReceiveMessage(connfd, &type, &src,0, 0, dv, nextHop) < 0)
                return -1;
            for(int i = 0; i < routerCnt; i++){
                if(i == src || VALID_DV(dv[i])){
                    printf("dest: %d, next: %d, cost: %d\n", routers[i].id, routers[nextHop[i]].id, dv[i]);
                }
            }
            close(connfd);
        }
        else if(cmd == RS_RESET){
            //printf("Sending RESET to router %d\n", src);
            if(ConnectRouter(&connfd, src) < 0)
                return -1;
            if(SendMessage(connfd, RS_RESET, -1, 0, 0, 0, 0) < 0)
                return -1;
            close(connfd);
        }
        else{
            //printf("Sending QUIT to all routers\n");
            for(int i = 0; i < routerCnt; i++){
                if(ConnectRouter(&connfd, i) < 0)
                    return -1;
                if(SendMessage(connfd, RS_QUIT, -1, 0, 0, 0, 0) < 0)
                    return -1;
                close(connfd);
            }
            return -1;
        }
    }
    return 0;
}

int ConnectRouter(int* sockfd, int i){
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(routers[i].port);
    if( inet_pton(AF_INET, routers[i].ip, &servaddr.sin_addr) <= 0){
        //printf("inet_pton error for %s\n",argv[1]);
        return -1;
    }
    if((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        //printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
        return -1;
    }
    if(connect(*sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){     
        //printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    return 0;
}

// read files from location
void ReadFiles(char* loc_path){
    // read location file 
    ifstream locationFile;
    locationFile.open(loc_path);
    locationFile >> routerCnt;
    for(int i = 0; i < routerCnt; i++){
        char c;
        int j = 0;
        for(; locationFile >> c;){
            if(c == ',') break;
            routers[i].ip[j++] = c;
        }
        routers[i].ip[j] = '\0';
        locationFile >> routers[i].port >> c >> routers[i].id;
    }
    locationFile.close();

    // make sure routers array is in ascending order
    sort(routers, routers+routerCnt, Router_Sorter);
    for(int i = 0; i < routerCnt; i++){
        routers[i].index = i;
        indexMap.insert(pair<int,int>(routers[i].id, i));
    }
}
int GetCommand(int* cmd, int* src, int* dest, int* cost){
    char c;
    char line[4096];
    string args[4] = {"","","",""};
    fgets(line, 4096, stdin);
    line[strcspn(line, "\n")] = 0;
    int j = 0;
    for(int i = 0; i < strlen(line); i++){
        c = line[i];
        if(c == ':' || c == ','){
            j++;
            continue;
        }
        args[j] += c;
    }
    cout << args[0] << " " << args[1] << " " << args[2] << " " << args[3] << endl;
    if(args[0] == "dv"){
        *cmd = RS_DV;
    }
    else if(args[0] == "update"){
        *src = stoi(args[1]);
        *dest = stoi(args[2]);
        *cost = stoi(args[3]);
        *src = indexMap[*src];
        *dest = indexMap[*dest];
        *cmd = RS_UPDATE;
    }
    else if(args[0]== "show"){
        *src = stoi(args[1]);
        *src = indexMap[*src];
        *cmd = RS_SHOW;
    }
    else if(args[0] == "reset"){
        *src = stoi(args[1]);
        *src = indexMap[*src];
        *cmd = RS_RESET;
    }
    else if(args[0] == "quit"){
        *cmd = RS_QUIT;
    }
    else{
        return -1;
    }
    return 0;
}