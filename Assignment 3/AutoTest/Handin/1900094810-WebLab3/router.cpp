#include "common.h"
/* =========== Globals =========== */
// sockets
int listenfd, connfd, routerfd[MAX_ROUTER] = {0}; // routerfd[i][0] : read, routerfd[i][0] : write
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
void ReadFiles(char* loc_path, char* top_path, int router_id);
void CheckData();

void ShowDV();
void InitDV();
int UpdateDV();
int SendDV();

int ConnectRouter(int* sockfd, int i);
int RouterMessageHandler(int src, int* dv);

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

    printf("Router %d - Listening\n", myRouter->index);
    // now wait for commands
    while(1){
        int type,src,dest,cost;
        int dv[MAX_ROUTER];
        if((connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1){
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            continue;
        }
        if(ReceiveMessage(connfd,&type,&src,&dest,&cost,dv,0) < 0)
            return -1;

        /* Message from router */
        if(type == RS_IRC){
            printf("Router %d - Received DV from router %d\n", myRouter->index, src); 
            if(RouterMessageHandler(src, dv) < 0)
                return -1;
        }
        /* Message from agent */
        else if(type == RS_DV){
            printf("Router %d - Received DV from agent\n", myRouter->index);
            if(SendDV() < 0) return -1; 
        }
        else if(type == RS_UPDATE){
            printf("Router %d - Received UPDATE from agent\n", myRouter->index);
            costTable[myRouter->index][dest] = cost;
            if(!routers[dest].neighbour && VALID_DV(cost)){
                routers[dest].neighbour = 1;
            }
            else if(routers[dest].neighbour && !VALID_DV(cost)){
                routers[dest].neighbour = 0;
            }
            UpdateDV(); 
        }
        else if(type == RS_SHOW){
            printf("Router %d - Received SHOW from agent\n", myRouter->index);
            for(int i = 0; i < routerCnt; i++){
                if(i == myRouter->index || VALID_DV(myRouter->dv[i])){
                    printf("dest: %d, next: %d, cost: %d\n", routers[i].id, routers[nextHop[i]].id, myRouter->dv[i]);
                }
            }
            SendMessage(connfd, RS_SHOW, myRouter->index, 0, 0, myRouter->dv, nextHop); 
        }
        else if(type == RS_RESET){
            printf("Router %d - Received RESET from agent\n", myRouter->index); 
            dvCount = 0;
        }
        else if(type == RS_QUIT){
            printf("Router %d - Received QUIT from agent\n", myRouter->index); 
            close(connfd);
            break;
        }
        else{
            return -1;
        }
        close(connfd);
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
        if(VALID_DV(myRouter->dv[i]))nextHop[i] = i;
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

    for(int to = 0; to < routerCnt; to++){
        if(to == myRouter->index)continue;
        for(int mid = 0; mid < routerCnt; mid++){
            if(routers[mid].neighbour == 0 || !VALID_DV(routers[mid].dv[to])) continue;
            if(!VALID_DV(myRouter->dv[to]) || routers[mid].dv[to] + costTable[myRouter->index][mid] < myRouter->dv[to]){
                myRouter->dv[to] = routers[mid].dv[to] + costTable[myRouter->index][mid];
                nextHop[to] = mid;
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

int SendDV(){
    ShowDV();
    for(int i = 0; i < routerCnt; i++){
        if(routers[i].neighbour){
            printf("Router %d - Sending DV to router %d\n", myRouter->index, i);
            int routerfd;
            if(ConnectRouter(&routerfd, i) < 0)
                return -1;
            if(SendMessage(routerfd, RS_IRC, myRouter->index, 0, 0, myRouter->dv, 0) < 0){
                return -1;
            }
            close(routerfd);
        }
    }
    return 0;
}


// received dv from router i
int RouterMessageHandler(int src, int* dv){
    dvCount++;
    // update neighbour's dv 
    for(int i = 0; i < MAX_ROUTER; i++){
        routers[src].dv[i] = dv[i];
    }

    // calculate dv
    if(UpdateDV()){
        if(SendDV() < 0)
            return -1;
    }
    return 0;
}

/* ========== Connections =========== */
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
    //printf("Connecting to router %d...\n", i);
    if(connect(*sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){     
        //printf("connect error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    return 0;
}



/* ========== Helper Functions ==========*/
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

