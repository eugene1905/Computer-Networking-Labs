#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>

#include "util.h"
#include "rtp.h"

int receiver(char *receiver_port, int window_size, char* file_name) {

  char buffer[RECV_BUFFER_SIZE];

  // create rtp socket file descriptor
  printf("Creating socket...\n");
  int receiver_fd = rtp_socket(window_size);
  if (receiver_fd == 0) {
    perror("create rtp socket failed");
    exit(EXIT_FAILURE);
  }

  // create socket address
  // forcefully attach socket to the port
  struct sockaddr_in address;
  memset(&address, 0, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(atoi(receiver_port));

  // bind rtp socket to address
  if (rtp_bind(receiver_fd, (struct sockaddr *)&address, sizeof(struct sockaddr))<0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  int recv_bytes;
  struct sockaddr_in sender;
  socklen_t addr_len = sizeof(struct sockaddr_in);

  // listen to incoming rtp connection
  if(rtp_listen(receiver_fd, 1) == -1){
      rtp_close(receiver_fd);
      return -1;
  }
    
  // accept the rtp connection
  rtp_accept(receiver_fd, (struct sockaddr*)&sender, &addr_len);

  // receive packet
  FILE *f = fopen(file_name, "wb+");
  uint32_t seq_begin = 0;
  uint32_t ack_record[MAX_WINDOW];
  while ((recv_bytes = rtp_recvfrom(receiver_fd, (void *)buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &addr_len, &seq_begin, ack_record)) > 0) { 
    uint32_t seq = seq_begin;
    int total_bytes = 0;
    //buffer[recv_bytes] = '\0'; 
    while(total_bytes < recv_bytes){
      fwrite(buffer+(seq-seq_begin)*PACKET_SIZE, ack_record[seq % MAX_WINDOW], 1, f);
      total_bytes += ack_record[seq % MAX_WINDOW];
      seq++;
    }
    if(total_bytes != recv_bytes)printf("total_bytes = %d, recv_bytes = %d\n", total_bytes, recv_bytes);
    memset(ack_record, 0, sizeof(ack_record));
    //fwrite(buffer, recv_bytes, 1, f); 
    //if(recv_bytes < TOTAL_PACKET * PACKET_SIZE) break;
  }
  fclose(f);
  //rtp_close(receiver_fd);
  return 0;
}

/*
 * main():
 * Parse command-line arguments and call receiver function
*/
int main(int argc, char **argv) {
    char *receiver_port;
    int window_size;
    char *file_name;

    if (argc != 4) {
        fprintf(stderr, "Usage: ./receiver [Receiver Port] [Window Size] [File Name]\n");
        exit(EXIT_FAILURE);
    }

    receiver_port = argv[1];
    window_size = atoi(argv[2]);
    file_name = argv[3];
    return receiver(receiver_port, window_size, file_name);
}
