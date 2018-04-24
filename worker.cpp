#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT_NUMBER 60519
#define WORKER_JOIN -1

int socket_connect(char * server_address, int port) {
  // Connect to its parent using directory server
  struct hostent* server = gethostbyname(server_address);
  if(server == NULL) {
    fprintf(stderr, "Unable to find host %s\n", server_address);
    return -1;
  }
  
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock == -1) {
    perror("socket failed");
    return -1;
  }

  struct sockaddr_in addr_client = {
    .sin_family = AF_INET,
    .sin_port = htons(port)
  };

  bcopy((char*)server->h_addr, (char*)&addr_client.sin_addr.s_addr, server->h_length);

  if(connect(sock, (struct sockaddr*)&addr_client, sizeof(struct sockaddr_in))) {
    perror("connect failed in socket_connect");
    return -1;
  }
  return sock;
}



int main(int argc, char** argv) {
  if(argc != 2) {
    fprintf(stderr, "Usage: %s <server address>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  char* server_address = argv[1];
  int server_socket = socket_connect(server_address, PORT_NUMBER);
  int server_socket_copy = dup(server_socket);
  if(server_socket_copy == -1) {
    perror("dup failed in directory_request");
    exit(EXIT_FAILURE);
  }

   // Open the socket as a FILE stream so we can use fgets
  FILE* input_dir = fdopen(server_socket, "r");
  FILE* output_dir = fdopen(server_socket_copy, "w");

  // Check for errors
  if(input_dir == NULL || output_dir == NULL) {
    perror("fdopen failed in directory_request");
    exit(EXIT_FAILURE);
  }

  fprintf(output_dir, "%d\n", WORKER_JOIN);
  fprintf(output_dir, "lol I\"m connecting");
  fflush(output_dir);

  //close
  close(server_socket);
  close(server_socket_copy);
  //end
}
