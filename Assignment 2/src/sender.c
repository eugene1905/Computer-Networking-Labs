#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.h"
#include "rtp.h"

int sender(char *receiver_ip, char* receiver_port, int window_size, char* message){

  // create socket
  int sock = 0;
  if ((sock = rtp_socket(window_size)) < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // create receiver address
  struct sockaddr_in receiver_addr;
  memset(&receiver_addr, 0, sizeof(receiver_addr));
  receiver_addr.sin_family = AF_INET;
  receiver_addr.sin_port = htons(atoi(receiver_port));

  // convert IPv4 or IPv6 addresses from text to binary form
  if(inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr)<=0) {
    perror("address failed");
    exit(EXIT_FAILURE);
  }

  // connect to server
  if(rtp_connect(sock, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr)) == -1){
    rtp_close(sock);
    return -1;
  }
    

  // send data

  // TODO: if message is filename, open the file and send its content
  FILE *f;
  if((f = fopen(message, "rb")) == NULL){
    rtp_sendto(sock, (void *)message, strlen(message), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));
  }
  else{
    char* buffer = 0;
    fseek (f, 0, SEEK_END);
    int length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
    rtp_sendto(sock, (void *)buffer, strlen(buffer), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));
  }
  
  // char test_data[] = "Hello, world!\n";
  // rtp_sendto(sock, (void *)test_data, strlen(test_data), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));

  // close rtp socket
  rtp_close(sock);
  return 0;
}



/*
 * main()
 * Parse command-line arguments and call sender function
*/
int main(int argc, char **argv) {
  char *receiver_ip;
  char *receiver_port;
  int window_size;
  char *message;

  if (argc != 5) {
    fprintf(stderr, "Usage: ./sender [Receiver IP] [Receiver Port] [Window Size] [message]");
    exit(EXIT_FAILURE);
  }

  receiver_ip = argv[1];
  receiver_port = argv[2];
  window_size = atoi(argv[3]);
  message = argv[4];
  return sender(receiver_ip, receiver_port, window_size, message);
}