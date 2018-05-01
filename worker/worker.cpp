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
#include <vector>
#include <string>
#include "../worker-server.hpp"

#define PORT_NUMBER 60519
#define WORKER_JOIN -1

// The pointer to the function in the library that will be executed
typedef int (*real_main_t)(int argc, char** argv);

// NOTE: relative path doesn't seem to work here
const char* shared_library = "../shared_library/d-map-injection-temp.so";

/**
 * Makes a socket and connect to another with the given address on the given port.
 * @param server_address, a string that represents the name of the server
 * @param port, an integer that represents the port number
 *
 * @returns file descriptor for the socket that is connected to server_address on port.
 */
int socket_connect(char * server_address, int port) {
  // Turn the server name into an IP address
  struct hostent* server = gethostbyname(server_address);
  if(server == NULL) {
    fprintf(stderr, "Unable to find host %s\n", server_address);
    return -1;
  }

  // Set up a socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock == -1) {
    perror("socket failed");
    return -1;
  }

  // Initialize the socket address
  struct sockaddr_in addr_client = {
    .sin_family = AF_INET,
    .sin_port = htons(port)
  };

  // Fill in the address from the server variable we declared earlier
  bcopy((char*)server->h_addr, (char*)&addr_client.sin_addr.s_addr, server->h_length);

  // Connect to the server
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
  
  // Connect to server
  char* server_address = argv[1];
  int server_socket = socket_connect(server_address, PORT_NUMBER);

  // Send a worker-join message to the server
  char result[50];
  sprintf(result, "%d\n", WORKER_JOIN);
  write(server_socket, result, strlen(result));

  // First get the size of the executable
  char executable_size[10];
  read(server_socket, (void*)executable_size, 10);
  long filesize = strtol(executable_size, NULL, 10);
  
  // Then get the executable file and save it locally
  char executable[filesize];
  int bytes_to_read = filesize;
  int prev_read = 0;

  // Open a temp file in the "write-binary" mode.
  FILE * exe_lib = fopen(shared_library, "wb");
  if (exe_lib == NULL) { 
    perror("Failed: ");
    exit(1);
  }

  // Keep reading bytes until the entire file is read.
  while (bytes_to_read > 0) {
    int executable_read = read(server_socket, (void*)executable, filesize);
    bytes_to_read -= executable_read;
    prev_read += executable_read;
  }
  
  // Write the read bytes to the file.
  if (fwrite(executable, filesize, 1, exe_lib) != 1){
    fprintf(stderr, "fwrite\n");
    exit(1);
  }
  
  // Get function arguments from the server
  task_arg_worker_t* buffer = (task_arg_worker_t*)malloc(sizeof(task_arg_worker_t));
  int bytes_read = read(server_socket, (void*)buffer, sizeof(task_arg_worker_t));
  if(bytes_read < sizeof(task_arg_worker_t)) {
    fprintf(stderr, "Read: Not reading enough bytes. Expected: %lu; Actual: %d", sizeof(task_arg_worker_t), bytes_read);
    exit(2);
  }
 
  // Unpack the function arguments sent from the server
  int num_args = buffer->num_args;
  char function_name[256];
  strcpy(function_name, buffer->function_name);
  char inputs[256];
  strcpy(inputs, buffer->inputs);
  int section_num = buffer->section_num;

  // Initialize the arguments to the program
  char* func_args[num_args];
  int index = 0;

  // The first argument to the function will be the name of the function
  func_args[0] = function_name;
  index++;

  // The next argument[s] are the input(s) to the function
  // TODO: Will change it to a list of inputs
  func_args[index] = inputs;
  index++;

  // The last argument to the function will be the section number
  char num[20];
  sprintf(num, "%d", section_num);
  func_args[index] = num;

  // Load the shared library (actual program resides here)
  void* injection = dlopen(shared_library, RTLD_LAZY | RTLD_GLOBAL);
  if(injection == NULL) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }
  
  // Get the entrance function
  real_main_t real_main = (real_main_t)dlsym(injection, function_name);
  char* error = dlerror();
  if(error != NULL) {
    printf("Error: %s\n", error);
    exit(1);
  }
  
  // Swap the server_socket in and use it as stdout 
  if(dup2(server_socket, STDOUT_FILENO) == -1) {
    fprintf(stderr, "Failed to set server socket as output\n");
    exit(2);
  } 
  
  // Execute the program
  real_main(num_args+2, func_args);
  
  //close
  close(server_socket);
  //end
}
