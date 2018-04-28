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
#include <dlfcn.h>
#include <errno.h>

#define PORT_NUMBER 60519
#define WORKER_JOIN -1

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

  char * func_args[3];
  
  // TODO: now it is passing the string "./worker", change it to sth else later
  // TODO: Why we need to save this thing?
  func_args[0] = argv[0];
  int index = 1;
  if(argc != 2) {
    fprintf(stderr, "Usage: %s <server address>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Connect to server
  char* server_address = argv[1];
  int server_socket = socket_connect(server_address, PORT_NUMBER);

  int server_socket_copy = dup(server_socket);
  if(server_socket_copy == -1) {
    perror("dup failed in client_thread_fn");
    exit(EXIT_FAILURE);
  }

  // Open the socket as a FILE stream so we can use fgets
  FILE* input = fdopen(server_socket, "r");
  FILE* output = fdopen(server_socket_copy, "w");
  
  // Check for errors
  if(output == NULL || input == NULL) {
    perror("fdopen failed in client_thread_fn");
    exit(EXIT_FAILURE);
  }

  fprintf(output, "%d\n", WORKER_JOIN);

  char * line = NULL;
  size_t linecap = 0;
  if (getline(&line, &linecap, input) > 0) {
    char * dup = strdup(line);
    func_args[2] = dup;
    index++;
  }
  char * pathname =  strcpy(pathname,"./client/passwords.txt");

  func_args[1] = pathname;
  
  // Load the shared library (actual program resides here)
  void* injection = dlopen(shared_library, RTLD_LAZY | RTLD_GLOBAL);
  if(injection == NULL) {
    perror("dlopen");
    exit(1);
  }

  // Clear error
  dlerror();
  // Get the entrance function
  real_main_t real_main = (real_main_t)dlsym(injection, "entrance");
  char* error = dlerror();
  if(error != NULL) {
    printf("Error: %s\n", error);
    exit(1);
  }

  real_main(3, func_args);
 
  
  //close
  close(server_socket);
  //end
}
