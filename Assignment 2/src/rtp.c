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
    while(rtp_recvstatus(sockfd, (struct sockaddr*)&sender, &addr_len) != RTP_START){
        printf("Server - Access Failed\n");
    }
    printf("Server - START REQUEST received\n");
    rtp_sendstatus(sockfd, (struct sockaddr*)&sender, addr_len, RTP_ACK);
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
    rtp_sendstatus(sockfd, addr, addrlen, RTP_START);
    struct sockaddr_in sender;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    if(rtp_recvstatus(sockfd, (struct sockaddr*)&sender, &addr_len) != RTP_ACK){
        printf("Not ACK yet\n");
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
    // TODO: send message

    char buffer[BUFFER_SIZE];
    rtp_header_t *rtp = (rtp_header_t *)buffer;
    rtp->length = len;
    rtp->checksum = 0;
    rtp->seq_num = 0;
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

int rtp_recvfrom(int sockfd, void *buf, int len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    // TODO: recv message

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

    // verify checksum
    uint32_t pkt_checksum = rtp->checksum;
    rtp->checksum = 0;
    uint32_t computed_checksum = compute_checksum(buffer, recv_bytes);
    if (pkt_checksum != computed_checksum)
    {
        perror("checksums not match");
        return -1;
    }

    memcpy(buf, buffer + sizeof(rtp_header_t), rtp->length);

    return rtp->length;
}

int rtp_sendstatus(int sockfd, const struct sockaddr *to, socklen_t tolen, int status){
    char buffer[BUFFER_SIZE];
    rtp_header_t *rtp = (rtp_header_t *)buffer;
    rtp->length = 0;
    rtp->checksum = 0;
    rtp->seq_num = 0;
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

int rtp_recvstatus(int sockfd, struct sockaddr *sender, socklen_t *addr_len){
    int recv_bytes;
    char buffer[BUFFER_SIZE];
    recv_bytes = recvfrom(sockfd, buffer, 2048, 0, sender, addr_len);
    if (recv_bytes < 0)
    {
        perror("receive error");
        exit(EXIT_FAILURE);
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
        return -1;
    }
    return rtp->type;
}
