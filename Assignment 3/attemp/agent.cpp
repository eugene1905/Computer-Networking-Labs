#include "common.h"

int routerCnt;
Router routers[MAX_ROUTER];
int routerfd[MAX_ROUTER];
map<int, int> indexMap;

void ReadFiles(char* loc_path);
int GetCommand(int* src, int* dest, int* cost);

int main(int argc, char** argv){
    setvbuf(stdout, NULL, _IONBF, 0); // to accommodate automated tests
    struct sockaddr_in servaddr;    

    if(argc != 2){
        printf("usage: ./agent <router location file>\n");
        return 0;
    }
    
    ReadFiles(argv[1]);
    
    for(int i = 0; i < routerCnt; i++){
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(routers[i].port);
        if( inet_pton(AF_INET, routers[i].ip, &servaddr.sin_addr) <= 0){
            //printf("inet_pton error for %s\n",argv[1]);
            return 0;
        }
        if((routerfd[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0){
            //printf("create socket error: %s(errno: %d)\n", strerror(errno),errno);
            return 0;
        }
        //printf("Connecting to router %d...\n", i);
        if(connect(routerfd[i], (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
            //printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
        }
        if(SendRAC(routerfd[i], RAC_CONN) < 0)
            return 0;
    }
    //printf("Connection with routers completed\n");
    // loop for sending commands
    while(1){
        int src, dest, cost;
        int dv[MAX_ROUTER] = {0}, nextHop[MAX_ROUTER] = {0};
        switch(GetCommand(&src, &dest, &cost)){
            case RAC_DV:
                for(int i = 0; i < routerCnt; i++)
                    if(SendRAC(routerfd[i], RAC_DV)<0)
                        goto end;
                break;
            case RAC_UPDATE:
                if(SendRAC(routerfd[src], RAC_UPDATE, dest, cost)<0)
                    goto end;
                break;
            case RAC_SHOW:
                int type;
                if(SendRAC(routerfd[src], RAC_SHOW) < 0)
                    goto end;
                if(RecvRAC(routerfd[src],&type, 0, 0, dv, nextHop) < 0)
                    goto end;
                if(type != RAC_SHOW)
                    goto end;
                for(int i = 0; i < routerCnt; i++){
                    if(i == src || VALID_DV(dv[i])){
                         printf("dest: %d, next: %d, cost: %d\n", routers[i].id, routers[nextHop[i]].id, dv[i]);
                    }
                }
                break;
            case RAC_RESET:
                if(SendRAC(routerfd[src], RAC_RESET) < 0)
                    goto end;
                break;
            case RAC_CLOSE:
                goto end;
            default:
                goto end;
        }
    }
end:
    for(int i = 0; i < routerCnt; i++){
        close(routerfd[i]);
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
int GetCommand(int* src, int* dest, int* cost){
    char c;
    char line[MAXLINE];
    string args[4] = {"","","",""};
    fgets(line, MAXLINE, stdin);
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
    if(args[0] == "dv"){
        return RAC_DV;
    }
    else if(args[0] == "update"){
        *src = stoi(args[1]);
        *dest = stoi(args[2]);
        *cost = stoi(args[3]);
        *src = indexMap[*src];
        *dest = indexMap[*dest];
        return RAC_UPDATE;
    }
    else if(args[0]== "show"){
        *src = stoi(args[1]);
        *src = indexMap[*src];
        return RAC_SHOW;
    }
    else if(args[0] == "reset"){
        *src = stoi(args[1]);
        *src = indexMap[*src];
        return RAC_RESET;
    }
    else if(args[0] == "quit"){
        return RAC_CLOSE;
    }
    return -1;
}