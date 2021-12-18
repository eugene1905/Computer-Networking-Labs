#include "common.h"
/* =========== Globals =========== */
// sockets
int listenfd, connfd, agentfd, routerfd[MAX_ROUTER] = {0}; // routerfd[i][0] : read, routerfd[i][0] : write
struct sockaddr_in servaddr;

// local router info
int routerCnt, edgeCnt, dvCount = 0;
Router *myRouter;


// glob routers info
Router routers[MAX_ROUTER];
int costTable[MAX_ROUTER][MAX_ROUTER] = {0};
int nextHop[MAX_ROUTER] = {0};
map<int, int> indexMap; //map router id to indexes

/*=========== Helper Functions ===========*/
void LogMessage(string msg);
void ReadFiles(char* loc_path, char* top_path, int router_id);
void CheckData();

void ShowDV();
void InitDV();
int UpdateDV();


int AcceptAgent();
int ReqConnection(int* sockfd, char* ip, int port);
int ConnectToRouter(int i);
int EstablishConnection();

int SendDV();
int AgentMessageHandler();
int RouterMessageHandler(int index);




int main(int argc, char** argv){
    //setvbuf(stdout, NULL, _IONBF, 0); // to accomodate automated tests
    if(argc != 4){
        printf("usage: ./router <router location file> <topology conf file> <router_id>\n");
        return 0;
    }

    ReadFiles(argv[1], argv[2], atoi(argv[3]));
    
    InitDV();

    CheckData(); // check error

    if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return 0;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(myRouter->port);

    if( bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return 0;
    }

    if( listen(listenfd, 20) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        return 0;
    }

    LogMessage("Listening...");
    if(AcceptAgent() < 0){
        LogMessage("Accept agent failed");
        return 0;
    }
    LogMessage("Connected to Agent");
    if(EstablishConnection() < 0){
        LogMessage("Establish connection failed");
        return 0;
    }
    LogMessage("Connection with routers established");
    
    // now wait for commands
    while(1){
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(agentfd, &fds);
        int maxfd = agentfd;
        for(int i = 0; i < routerCnt; i++){
            if(routers[i].neighbour == 0)continue;
            FD_SET(routerfd[i], &fds);
            maxfd = maxfd > routerfd[i]? maxfd : routerfd[i];
        }
        select(maxfd+1, &fds, NULL, NULL, NULL);
        if(FD_ISSET(agentfd, &fds)){
            printf("Router %d - Message Received from agent \n", myRouter->index);
            if(AgentMessageHandler() < 0)break;
        }
        for(int i = 0; i < routerCnt; i++){
            if(FD_ISSET(routerfd[i], &fds)){
                printf("Router %d - Message Received from Router %d\n", myRouter->index,i);
                if(RouterMessageHandler(i) < 0){
                    close(routerfd[i]);
                    routerfd[i] = 0;
                    break;
                }
            }
        }
    }
    close(agentfd);
    for(int i = 0; i < routerCnt; i++){
        if(routers[i].neighbour)
            close(routerfd[i]);
    }
    close(listenfd);
    return 0;
}


/* ========== DV operation ==========*/
void ShowDV(){
    printf("\nShow All DV\n");
    for(int i = 0; i < routerCnt; i++){
        if(routers[i].neighbour == 0 && myRouter->index != i)continue;
        printf("%d: ",i);
        for(int j = 0; j < routerCnt; j++){
            printf("%d ", routers[i].dv[j]);
        }
        printf("\n");
    }
}

// init local & neighbours' DV
void InitDV(){
    for(int i = 0; i < routerCnt; i++){
        if(i == myRouter->index){
            nextHop[i] = i;
            continue;
        }
        myRouter->dv[i] = costTable[myRouter->index][i];
        if(myRouter->dv[i] > 0)nextHop[i] = i;
    }
    for(int i = 0; i < routerCnt; i++){
        if(routers[i].neighbour == 0)continue;
        for(int j = 0; j < routerCnt; j++){
            routers[i].dv[j] =  (i==j ? 0 : -1);
        }
    }
}

// return 1 if host's DV changed
int UpdateDV(){
    int isChanged = 0;
    int prev[MAX_ROUTER] = {0};
    for(int i = 0; i < MAX_ROUTER; i++)
        prev[i] = myRouter->dv[i];
    
    for(int i = 0; i < routerCnt; i++){
        if(i == myRouter->index){
            nextHop[i] = i;
            continue;
        }
        myRouter->dv[i] = costTable[myRouter->index][i];
        if(VALID_DV(myRouter->dv[i]))nextHop[i] = i;
        else nextHop[i] = 0;
    }
    for(int i = 0; i < routerCnt; i++){
        if(i == myRouter->index)continue;
        for(int j = 0; j < routerCnt; j++){
            if(routers[j].neighbour == 0 || !VALID_DV(routers[j].dv[i])) continue;
            if(myRouter->dv[i] <= 0 || routers[j].dv[i] + costTable[myRouter->index][j] < myRouter->dv[i]){
                myRouter->dv[i] = routers[j].dv[i] + costTable[myRouter->index][j];
                nextHop[i] = j;
            }
        }
    }
    for(int i = 0; i < MAX_ROUTER; i++){
        if(!VALID_DV(prev[i]) && !VALID_DV(myRouter->dv[i]))
            continue;
        if(myRouter->dv[i] != prev[i])
            isChanged = 1;
    }
    ShowDV();
    return isChanged;
}

/* ========== Commands Handler ========== */
// agent's commands
int SendDV(){
    ShowDV();
    for(int i = 0; i < routerCnt; i++){
        if(routers[i].neighbour){
            printf("Router %d - Sending DV to router %d\n", myRouter->index, i);
            if(SendIRC(routerfd[i], IRC_DV, myRouter->id, myRouter->dv) < 0){
                return -1;
            }
        }
    }
    return 0;
}

// received commands from agent
int AgentMessageHandler(){
    int type,dest, cost;
    if(RecvRAC(agentfd, &type, &dest, &cost, 0) < 0){
        printf("error: AgentMessageHandler\n");
        return -1;
    }
        
    switch(type){
        case RAC_DV:
            printf("Router %d - Received DV from agent\n", myRouter->index); 
            SendDV();
            break;
        case RAC_UPDATE:
            printf("Router %d - Received UPDATE %d %d from agent\n", myRouter->index, dest, cost); 
            if(cost > 1000) cost = -1;
            costTable[myRouter->index][dest] = cost;
            if(!routers[dest].neighbour && VALID_DV(cost) > 0){
                routers[dest].neighbour = 1;
                ConnectToRouter(dest);
            }
            else if(routers[dest].neighbour && !VALID_DV(cost)){
                routers[dest].neighbour = 0;
                if(routerfd[dest] != 0)close(routerfd[dest]);
            }
            UpdateDV();
            break;
        case RAC_SHOW:
            printf("Router %d - Received SHOW from agent\n", myRouter->index);
            for(int i = 0; i < routerCnt; i++){
                if(myRouter->dv[i] >= 0){
                    printf("dest: %d, next: %d, cost: %d\n", routers[i].id, routers[nextHop[i]].id, myRouter->dv[i]);
                }
            }
            SendRAC(agentfd, RAC_SHOW, 0, 0, myRouter->dv, nextHop);
            break;
        case RAC_RESET:
            printf("Router %d - Received DV from agent\n", myRouter->index); 
            routerCnt = 0;
            break;
        case RAC_CLOSE:
            return -1;
        default:
            printf("Router %d - Unknown command from agent\n", myRouter->index);
            return -1;
    }
    return 0;
}

// received dv from router i
int RouterMessageHandler(int idx){
    int type, src;
    int dv[MAX_ROUTER];
    if(RecvIRC(routerfd[idx],&type, &src, dv) < 0)
        return -1;
    if(type != IRC_DV)
        return -1;
    printf("Router %d - Received DV from router %d\n", myRouter->index, idx); 
    dvCount++;

    // update neighbour's dv 
    for(int i = 0; i < MAX_ROUTER; i++){
        routers[idx].dv[i] = dv[i];
    }
    // calculate dv
    if(UpdateDV()){
        if(SendDV() < 0)
            return -1;
    }
    return 0;
}

/* ========== Connections =========== */
int AcceptAgent(){
     while(1){
        if( (connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1){
            //printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            continue;
        }
        int type;
        if(RecvRAC(connfd, &type, 0, 0, 0) < 0)
            return -1;
        if (type != RAC_CONN){
            close(connfd);
            continue;
        }
        printf("Router %d - Connection from Agent established\n",myRouter->index);
        agentfd = connfd;
        break;
    }
    return 0;
}

// return 1 on success
int ReqConnection(int* sockfd, char* ip, int port){
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if( inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0){
        printf("inet_pton error for %s\n", ip);
        return -1;
    }

    if( connect(*sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
        //printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    return 0;
}

int ConnectToRouter(int i){
    if(routerfd[i] != 0){
        printf("Router %d - Connection with router %d already established\n", myRouter->index, i);
        return 0; 
    }
    if(myRouter->index < i){
        if((routerfd[i] = socket(AF_INET,SOCK_STREAM,0)) < 0){
                printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
                exit(-1);
        }
        printf("Router %d - Connecting to Router %d\n", myRouter->index,i);    
        while(1){
            if(ReqConnection(&routerfd[i], routers[i].ip, routers[i].port) < 0){
                return -1;
            }
            printf("Router %d - Sending CONN to router %d\n", myRouter->index, i);
            if(SendIRC(routerfd[i], IRC_CONN, myRouter->index, myRouter->dv) < 0)
                return -1;
            break;
        }
    }
    else{
        printf("Router %d - Waiting Connection from Router %d\n",myRouter->index, i);
        while(1){
            if( (connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1){
                printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
                continue;
            }
            int type, src = 0;
            if(RecvIRC(connfd,&type, &src, 0) < 0)
                continue;
            if(type != IRC_CONN){
                close(connfd);
                continue;
            }
            if(routers[src].neighbour){
                printf("Router %d - Connection from Router %d established\n", myRouter->index, src);
                routerfd[src] = connfd;
            }
            else{
                close(connfd);
            }
            if(src == i)break;
        }
    }
    return 0;
}
// establish connection with neighbours
int EstablishConnection(){
    for(int i = 0; i < routerCnt; i++){
        if(routers[i].neighbour){
            if(ConnectToRouter(i) < 0)
                return -1;
        }
    }
    return 0;
}

/* ========== Helper Functions ==========*/
void LogMessage(string msg){
    cout << "Router " << myRouter->index <<  " - "  << msg << endl;
}

// read files from location & topology files
void ReadFiles(char* loc_path, char* top_path, int router_id){
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
        if(routers[i].id == router_id){
            myRouter = &routers[i];
        }
        routers[i].index = i;
        indexMap.insert(pair<int,int>(routers[i].id, i));
    }
    for(int i = 0; i < MAX_ROUTER; i++){
        for(int j = 0; j < MAX_ROUTER; j++){
            if(i == j) continue;
            costTable[i][j] = -1;
        }
    }
    // read topology file
    ifstream topologyFile;
    topologyFile.open(top_path);
    topologyFile >> edgeCnt;
    for(int i = 0; i < edgeCnt; i++){
        char c;
        int from, to;
        topologyFile >> from >> c >> to >> c;
        from = indexMap[from], to = indexMap[to];
        topologyFile >> costTable[from][to];
        if(from == myRouter->index && VALID_DV(costTable[from][to]))
            routers[to].neighbour = 1;
    }
    topologyFile.close();
}

void CheckData(){
    printf("Check Head Info\n");
    printf("%d: %s, %d, %d\n",myRouter->index, myRouter->ip, myRouter->port, myRouter->id);
    printf("\nCheck Location File\n");
    for(int i = 0; i < routerCnt; i++){
        printf("%d: %s,%d,%d\n",routers[i].index, routers[i].ip,routers[i].port, routers[i].id);
    }
    printf("\nCheck Topology File\n");
    for(int i = 0; i < routerCnt; i++){
        for(int j = 0; j < routerCnt; j++){
            printf("%d ",costTable[i][j]);
        }
        printf("\n");
    }

    printf("\nCheck Neighbours\n");
    for(int i = 0; i < routerCnt; i++){
        if(routers[i].neighbour)
            printf("%d ", routers[i].id);
    }
    printf("\n");
    for(int i = 0; i < routerCnt; i++){
        if(routers[i].neighbour)
            printf("%d ", routers[i].index);
    }
    printf("\n");

    ShowDV();
}

