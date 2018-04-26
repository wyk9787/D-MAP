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
#define WORKER_JOIN "-1"

typedef int (*real_main_t)(int argc, char** argv);
const char* shared_library = "../shared_library/d-map-injection.so";

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

  char msg[3];
  msg[0] = '1';
  msg[1] = '\n';
  msg[2] = '\0';
  printf(msg);
  write(server_socket, msg, strlen(msg));

  char buffer[256];
  int bytes_read = read(s, buffer, 256);
  if(bytes_read < 0) {
    perror("read failed");
    exit(2);
  }

  int input = atoi(buffer);

  errno = 0;
  void* injection = dlopen(shared_library, RTLD_LAZY | RTLD_GLOBAL);
  if(injection == NULL) {
    perror("dlopen");
    exit(1);
  }
  dlerror();
  real_main_t real_main = (real_main_t)dlsym(injection, "entrance");

  real_main(1, input);
  char* error = dlerror();
  if(error != NULL) {
    printf("Error: %s\n", error);
    exit(1);
  }

  
  
  //close
  close(server_socket);
  //end
}
