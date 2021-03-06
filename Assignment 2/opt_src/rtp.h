#ifndef RTP_H
#define RTP_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#define RTP_START 0
#define RTP_END   1
#define RTP_DATA  2
#define RTP_ACK   3

#define RECV_BUFFER_SIZE 32768  // 32KB
#define BUFFER_SIZE 2048
#define PACKET_SIZE (1472-11)
#define MAX_PACKET 4294967295
#define MAX_WINDOW 500
#define TOTAL_PACKET (RECV_BUFFER_SIZE / PACKET_SIZE)

typedef struct __attribute__ ((__packed__)) RTP_header {
    uint8_t type;       // 0: START; 1: END; 2: DATA; 3: ACK
    uint16_t length;    // Length of data; 0 for ACK, START and END packets
    uint32_t seq_num;
    uint32_t checksum;  // 32-bit CRC
} rtp_header_t; // 11 bytes


typedef struct RTP_control_block {
    uint32_t window_size;
    // TODO: you can add your RTP-related fields here
    uint32_t ack_record[MAX_WINDOW];
    uint32_t seq;
} rcb_t;

static rcb_t* rcb = NULL;

// different from the POSIX
int rtp_socket(uint32_t window_size);

int rtp_listen(int sockfd, int backlog);

int rtp_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int rtp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen);

int rtp_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int rtp_close(int sockfd);

int rtp_sendto(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen);

int rtp_recvfrom(int sockfd, void *buf, int len, int flags, struct sockaddr *from, socklen_t *fromlen, uint32_t *seq_begin, uint32_t *ack_record);

int rtp_sendstatus(int sockfd, const struct sockaddr *to, socklen_t tolen, int status, int seq);

int rtp_recvstatus(int sockfd, struct sockaddr *from, socklen_t *fromlen, int *seq);

int rtp_sendone(int sockfd, const void *msg, int len, int flags, const struct sockaddr *to, socklen_t tolen, int seq);

#endif //RTP_H
