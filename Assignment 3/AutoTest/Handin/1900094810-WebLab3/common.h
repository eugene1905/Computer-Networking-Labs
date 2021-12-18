#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_ROUTER  10
#define MAX_COST    1000
#define RS_SIZE     (8 * MAX_ROUTER + 13) 

#define RS_IRC      1
#define RS_DV       2
#define RS_UPDATE   3
#define RS_SHOW     4
#define RS_RESET    5
#define RS_QUIT     6

#define VALID_DV(x) (x > 0 && x <= MAX_COST)

using namespace std;

// Routing simulation
typedef struct __attribute__ ((__packed__)) RS_header { 
    int8_t type;
    int32_t src;
    int32_t dest;
    int32_t cost;
    int32_t dv[MAX_ROUTER];
    int32_t nextHop[MAX_ROUTER];
} rs_header_t;

struct Router{
    int index;
    int neighbour;
    char ip[INET_ADDRSTRLEN];
    int port;
    int id;
    int dv[MAX_ROUTER];
};

bool Router_Sorter(Router const& lhs, Router const& rhs){
    return lhs.id < rhs.id;
}

int SendMessage(int sockfd, int type, int src, int dest, int cost, int* dv, int* nextHop){
    char buf[RS_SIZE];
    rs_header_t* rs = (rs_header_t *) buf;
    rs->type = type;
    rs->src = src;
    rs->dest = dest;
    rs->cost = cost;
    if(dv){
        for(int i = 0; i < MAX_ROUTER; i++){
            rs->dv[i] = dv[i]; 
        }
    }
    if(nextHop){
        for(int i = 0; i < MAX_ROUTER; i++){
            rs->nextHop[i] = nextHop[i];
        }
    }
    if(send(sockfd, (void *)buf, RS_SIZE, 0) <= 0)
        return -1;
    return 1;
}

int ReceiveMessage(int sockfd, int* type, int* src, int* dest, int* cost, int* dv , int * nextHop){
    char buf[RS_SIZE];
    if(recv(sockfd, buf, RS_SIZE, 0) <= 0)
        return -1;
    rs_header_t* rs = (rs_header_t *) buf;
    if(type)*type = rs->type;
    if(src)*src = rs->src;
    if(dest)*dest = rs->dest;
    if(cost)*cost = rs->cost;
    if(dv){
        for(int i = 0; i < MAX_ROUTER; i++){
            dv[i] = rs->dv[i]; 
        }
    }
    if(nextHop){
        for(int i = 0; i < MAX_ROUTER; i++){
            nextHop[i] = rs->nextHop[i];
        }
    }
    return 0;
}

#endif