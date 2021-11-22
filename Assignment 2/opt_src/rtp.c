#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>

#include "rtp.h"
#include "util.h"

void rcb_init(uint32_t window_size)
{
    if (rcb == NULL)
    {
        rcb = (rcb_t *)calloc(1, sizeof(rcb_t));
    }
    else
    {
        perror("The current version of the rtp protocol only supports a single connection");
        exit(EXIT_FAILURE);
    }
    rcb->window_size = window_size;
    // TODO: you can initialize your RTP-related fields here
}

/*********************** Note ************************/
/* RTP in Assignment 2 only supports single connection.
/* Therefore, we can initialize the related fields of RTP when creating the socket.
/* rcb is a global varialble, you can directly use it in your implementatyion.
/*****************************************************/
int rtp_socket(uint32_t window_size)
{
    rcb_init(window_size);
    // create UDP socket
    return socket(AF_INET, SOCK_DGRAM, 0);
}

int rtp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen)
{
    return bind(sockfd, addr, addrlen);
}

int rtp_listen(int sockfd, int backlog)
{
    // TODO: listen for the START message from sender and send back ACK
    // In standard POSIX API, backlog is the number of connections allowed on the incoming queue.
    // For RTP, backlog is always 1
    printf("Server - Listening\n");

    struct sockaddr_in sender;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int seq, ret;
    while((ret = rtp_recvstatus(sockfd, (struct sockaddr*)&sender, &addr_len, &seq)) != RTP_START){
        if(ret == -2){
            printf("Server - checksum error\n");
            return -1;
        }
        else if(ret != -1){
            printf("Server - Access Failed\n");
            return -1;
        }
    }
    printf("Server - START REQUEST received\n");
    rtp_sendstatus(sockfd, (struct sockaddr*)&sender, addr_len, RTP_ACK, seq);
    printf("Server - ACK sent\n");
    return 1;
}

int rtp_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    // Since RTP in Assignment 2 only supports one connection,
    // there is no need to implement accpet function.
    // You don’t need to make any changes to this function.
    return 1;
}

int rtp_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    // TODO: send START message and wait for its ACK
    printf("Sender - Sending Start\n");
    rtp_sendstatus(sockfd, addr, addrlen, RTP_START, 0);
    struct sockaddr_in sender;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int seq;
    if(rtp_recvstatus(sockfd, (struct sockaddr*)&sender, &addr_len, &seq) != RTP_ACK){
        printf("Sender - ACK not received\n");
        rtp_sendstatus(sockfd, addr, addrlen, RTP_END, 0);
        return -1;
    }
    printf("Sender - ACK received, Connection established\n");
    return 1;
}

int rtp_close(int sockfd)
{
    return close(sockfd);
}

    // In standard POSIX API, backlog is the number of connections allowed on the incoming queue.
int rtp_sendto(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen)
{
    int ACK_record[MAX_PACKET] = {0};
    memset(ACK_record,0 ,sizeof(ACK_record));
    // TODO: send message
    struct sockaddr_in sender;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int seq;
    int N = len / PACKET_SIZE + 1;
    int winStart = 0;
    printf("Sender - Starting to send packet...\n");
    while(winStart < N){
        // SEND WINDOW       
        int winEnd = winStart + rcb->window_size;
        if(winEnd > N) winEnd = N;
        for(int i = winStart; i < winEnd; i++){
            if(ACK_record[i] == 1)continue;
            printf("Sender - Sending packet %d...\n",i );
            char buffer[BUFFER_SIZE];
            int newlen = (i == N? len % PACKET_SIZE : PACKET_SIZE);
            memcpy(buffer, msg+i*PACKET_SIZE, newlen);
            rtp_sendone(sockfd, buffer, newlen, flags, to, tolen, i);
        }
        int flag = 1;
        int ret;
        CLEAR_BUFF:
        if((ret = rtp_recvstatus(sockfd, (struct sockaddr*)&sender, &addr_len, &seq)) == RTP_ACK){
            printf("ACK %d received\n",seq);
            winStart = seq;
            for(int i = 0; i < winStart; i++)
                ACK_record[i] = 1;
            flag = 0;
            goto CLEAR_BUFF;
        }
        else if(ret == -1){
            if(flag)
                printf("Sender - Timeout, resending...\n");
        }
        else if(ret == -2){
            printf("Sender - Checksum error\n");
        }            
    }
    printf("Sender - Sending END request...\n");
    rtp_sendstatus(sockfd, to, tolen, RTP_END, winStart);
    if(rtp_recvstatus(sockfd, (struct sockaddr*)&sender, &addr_len, &seq) != RTP_ACK){
        printf("Sender - ACK not received\n");
        return -1;
    }
    return 1;
}

int rtp_recvfrom(int sockfd, void *buf, int len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    // TODO: recv message
    int seq = 0; // seq_num expecting
    int last_seq = 0;
    int total_len = 0;
    int flag = 1;
    while(flag || seq < last_seq){
        char buffer[2048];
        int recv_bytes = recvfrom(sockfd, buffer, 2048, flags, from, fromlen);
        if (recv_bytes < 0)
        {
            perror("receive error");
            exit(EXIT_FAILURE);
        }
        buffer[recv_bytes] = '\0';
        

        // extract header
        rtp_header_t *rtp = (rtp_header_t *)buffer;
        printf("Server - Packet %d Received\n", rtp->seq_num);

        // verify checksum
        uint32_t pkt_checksum = rtp->checksum;
        rtp->checksum = 0;
        uint32_t computed_checksum = compute_checksum(buffer, recv_bytes);
        if (pkt_checksum != computed_checksum)
        {
            perror("checksums not match");
            continue;
            //return -1;
        }
        
        if(rtp->seq_num >= seq + rcb->window_size)continue;
        if(rtp->seq_num == seq){
            seq++;
        }
        if(rtp->type == RTP_END){
            flag = 0;
            last_seq = rtp->seq_num;
            printf("Server - END request received\n");
            printf("Server - Sending Ack %d\n", rtp->seq_num);
            rtp_sendstatus(sockfd, (struct sockaddr*)from, *fromlen, RTP_ACK, rtp->seq_num);
        }
        else{
            memcpy(buf+ rtp->seq_num * PACKET_SIZE, buffer + sizeof(rtp_header_t), rtp->length);
            total_len += rtp->length;
            printf("Server - Sending Ack %d\n", seq);
            rtp_sendstatus(sockfd, (struct sockaddr*)from, *fromlen, RTP_ACK, seq);
        }
        
        
    }
    
    return total_len;
}

/* ==============================  Helper Functions ============================== */
int rtp_sendstatus(int sockfd, const struct sockaddr *to, socklen_t tolen, int status, int seq){
    char buffer[BUFFER_SIZE];
    rtp_header_t *rtp = (rtp_header_t *)buffer;
    rtp->length = 0;
    rtp->checksum = 0;
    rtp->seq_num = seq;
    rtp->type = status;
    rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t));

    int sent_bytes = sendto(sockfd, (void *)buffer, sizeof(rtp_header_t), 0, to, tolen);
    if (sent_bytes != (sizeof(struct RTP_header)))
    {
        perror("send status error");
        exit(EXIT_FAILURE);
    }
    return 1;
}

int rtp_recvstatus(int sockfd, struct sockaddr *sender, socklen_t *addr_len, int* seq){
    // Init for select() 
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    int recv_bytes;
    char buffer[BUFFER_SIZE];
    select(sockfd+1, &readfds, NULL, NULL, &tv);
        // TIMEOUT
    if(FD_ISSET(sockfd, &readfds)){
        recv_bytes = recvfrom(sockfd, buffer, 2048, 0, sender, addr_len);
        if (recv_bytes < 0)
        {
            perror("receive error");
            exit(EXIT_FAILURE);
        }
    }
    else{
        return -1;
    }
    
    buffer[recv_bytes] = '\0';
    // extract header
    rtp_header_t *rtp = (rtp_header_t *)buffer;

    // verify checksum
    uint32_t pkt_checksum = rtp->checksum;
    rtp->checksum = 0;
    uint32_t computed_checksum = compute_checksum(buffer, recv_bytes);
    if (pkt_checksum != computed_checksum)
    {
        perror("checksums not match");
        return -2;
    }
    *seq = rtp->seq_num;
    return rtp->type;
}

int rtp_sendone(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen, int seq){
    char buffer[BUFFER_SIZE];
    rtp_header_t *rtp = (rtp_header_t *)buffer;
    rtp->length = len;
    rtp->checksum = 0;
    rtp->seq_num = seq;
    rtp->type = RTP_DATA;
    memcpy((void *)buffer + sizeof(rtp_header_t), msg, len);
    rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t) + len);

    int sent_bytes = sendto(sockfd, (void *)buffer, sizeof(rtp_header_t) + len, flags, to, tolen);
    if (sent_bytes != (sizeof(struct RTP_header) + len))
    {
        perror("send error");
        exit(EXIT_FAILURE);
    }
    return 1;
}