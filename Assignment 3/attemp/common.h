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

#define MAX_ROUTER  15
#define MAX_COST    1000
#define MAXLINE     4096
#define RAC_SIZE    (6 * MAX_ROUTER + 11 + 100)
#define IRC_SIZE    (2 * MAX_ROUTER + 5 + 100)

#define RAC_CONN     0
#define RAC_DV       1
#define RAC_UPDATE   2
#define RAC_SHOW     3
#define RAC_RESET    4
#define RAC_CLOSE    5

#define IRC_CONN     0
#define IRC_DV       1
#define IRC_CLOSE    2

#define VALID_DV(x) (x > 0 && x <= MAX_COST)

using namespace std;

// Router-Agent Communication
typedef struct __attribute__ ((__packed__)) RAC_header { 
    uint8_t type;       
    int32_t dest;
    int32_t cost;
    int32_t dv[MAX_ROUTER];
    int32_t nextHop[MAX_ROUTER];    
} rac_header_t;

// Inter-Router Communication 
typedef struct __attribute__ ((__packed__)) IRC_header { 
    uint8_t type;       
    uint32_t src;    
    int32_t dv[MAX_ROUTER];
} irc_header_t;


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

int SendRAC(int sockfd, int type, int dest = 0, int cost = 0, int* dv = 0, int* nextHop = 0){
    char buf[RAC_SIZE];
    rac_header_t* rac = (rac_header_t *) buf;
    rac->type = type;
    rac->dest = dest;
    rac->cost = cost;
    if(dv){
        for(int i = 0; i < MAX_ROUTER; i++){
            rac->dv[i] = dv[i]; 
        }
    }
    if(nextHop){
        for(int i = 0; i < MAX_ROUTER; i++){
            rac->nextHop[i] = nextHop[i];
        }
    }
    if(send(sockfd, (void *)buf, RAC_SIZE, 0) <= 0)
        return -1;
    return 1;
}

int RecvRAC(int sockfd, int* type, int* dest, int* cost, int* dv = 0, int * nextHop = 0){
    char buf[RAC_SIZE];
    if(recv(sockfd, buf, RAC_SIZE, 0) <= 0)
        return -1;
    rac_header_t* rac = (rac_header_t *) buf;
    if(type)*type = rac->type;
    if(dest)*dest = rac->dest;
    if(cost)*cost = rac->cost;
    if(dv){
        for(int i = 0; i < MAX_ROUTER; i++){
            dv[i] = rac->dv[i]; 
        }
    }
    if(nextHop){
        for(int i = 0; i < MAX_ROUTER; i++){
            nextHop[i] = rac->nextHop[i];
        }
    }
    return 0;
}

int SendIRC(int sockfd, int type, int src, int* dv){
    char buf[IRC_SIZE];
    irc_header_t* irc = (irc_header_t *) buf;
    irc->type = type;
    irc->src = src;
    for(int i = 0; i < MAX_ROUTER; i++)
        irc->dv[i] = dv[i];
    if(send(sockfd, (void *)buf, IRC_SIZE, 0) <= 0)
        return -1;
    return 0;
}

int RecvIRC(int sockfd, int* type, int* src, int* dv){
    char buf[IRC_SIZE];
    if(recv(sockfd, buf, IRC_SIZE, 0) <= 0)
        return -1;
    irc_header_t* irc = (irc_header_t *) buf;
    if(type)*type = irc->type;
    if(src)*src = irc->src;
    if(dv){
        for(int i = 0; i < MAX_ROUTER; i++)
            dv[i] = irc->dv[i];
    }
    return 0;
}

#endif