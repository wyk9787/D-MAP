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
#include "worker-server.hpp"

#define MAX_COMMAND_WIDTH 100
#define MAX_NUM_COMMAND 100

int socket_connect(char* server_address, int port) {
  struct hostent* server = gethostbyname(server_address);
  if(server == NULL) {
    fprintf(stderr, "Unable to find host %s\n", server_address);
    exit(EXIT_FAILURE);
  }

  // Set up a socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock == -1) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // Initialize the socket address
  struct sockaddr_in addr_client = {
    .sin_family = AF_INET,
    .sin_port = htons(port)
  };

  bcopy((char*)server->h_addr, (char*)&addr_client.sin_addr.s_addr, server->h_length);

  // Connect to the server
  if(connect(sock, (struct sockaddr*)&addr_client, sizeof(struct sockaddr_in))) {
    perror("connect failed in socket_connect");
    return -1;
  }

  char join[5];
  sprintf(join, "%d", USER_JOIN);

  // Send join message
  if(write(sock, join, strlen(join)) == -1) {
    perror("write");
    exit(EXIT_FAILURE);
  }
  return sock;
}


int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <server address>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  char* server_address = argv[1];
  int server_socket = socket_connect(server_address, D_SERVER_PORT);

  char* program_path = NULL;
  size_t program_len;
  printf("Please enter the path of the program (.so file): \n");
  if (getline(&program_path, &program_len, stdin) == -1) {
    fprintf(stderr, "getline() failed\n");
    exit(EXIT_FAILURE);   
  }
  program_path[strlen(program_path)-1] = '\0';

  printf("Please enter the function name:\n");
  char* function_name = NULL;
  size_t function_name_len;
  if (getline(&function_name, &function_name_len, stdin) == -1) {
    fprintf(stderr, "getline() failed\n");
    exit(EXIT_FAILURE);   
  }
  function_name[strlen(function_name)-1] = '\0';
    
  char* line = NULL;
  size_t line_size;
  
  printf("Please enter all the command line arguments for each program\n");
  printf("Enter 'NULL' when you are done.\n");
  if (getline(&line, &line_size, stdin) == -1) {
    fprintf(stderr, "getline() failed\n");
    exit(EXIT_FAILURE);   
  }
  line[strlen(line)-1] = '\0';

  char commands[MAX_NUM_COMMAND][MAX_COMMAND_WIDTH];
  int num_args = 0; // Index for the commands
  while(strcmp(line, "NULL") != 0) { // Still have more commands
    // We need to get rid of the newline character
    strncpy(commands[num_args], line, strlen(line)); 
    printf("line: %s\n", commands[num_args]);
    num_args++;

    line = NULL;
    line_size = 0;
    if (getline(&line, &line_size, stdin) == -1) {
      fprintf(stderr, "getline() failed\n");
      exit(EXIT_FAILURE);   
    }
    line[strlen(line)-1] = '\0';
  }

  // Open the exectuable file
  FILE* exec_file = fopen(program_path, "rb");
  if(exec_file == NULL) {
    fprintf(stderr, "Read file %s failed\n", program_path);
    exit(EXIT_FAILURE);
  }

  // Jump to the end of the file
  fseek(exec_file, 0, SEEK_END);          
  // Get the current byte offset in the file
  long filelen = ftell(exec_file);             
  // Go back to the beginning of the file 
  rewind(exec_file);

  // Create space for the file 
  char* buffer = (char *)malloc((filelen+1)*sizeof(char)); // Enough memory for file + \0
  // Read file into the buffer
  fread(buffer, filelen, 1, exec_file); // Read in the entire file
  fclose(exec_file); // Close the file

  // Send file size
  char size_message[10];
  sprintf(size_message, "%ld", filelen);
  printf("size len: %s\n", size_message);
  if(write(server_socket, size_message, 10) == -1) {
    perror("write");
    exit(EXIT_FAILURE);
  }
 
  // Send actual file (exectuable)
  int write_bytes;
  write_bytes = write(server_socket, buffer, filelen); 
  if(write_bytes== -1) {
    perror("write");
    exit(EXIT_FAILURE);
  }
  printf("Write %d bytes\n", write_bytes);

  // Send arguments
  task_arg_user_t args = {
    .num_args = num_args 
  };
  strncpy(args.function_name, function_name, strlen(function_name));
  strncpy(args.inputs, commands[0], strlen(commands[0]));

  printf("num_args: %d, function_name: %s, inputs: %s\n", args.num_args, args.function_name, args.inputs);
  if(write(server_socket, &args, sizeof(task_arg_user_t)) == -1) {
    perror("write");
    exit(EXIT_FAILURE);
  }
  
  
  // Reading program's output
  char size_buffer[10];

  while (true) {
    int bytes_read = read(server_socket, size_buffer, 10);

    // Save the size of the output
    int bytes_to_read = atoi(size_buffer);

    //if there is nothing left to read
    if (bytes_to_read == 0)
      break;

    char print_buffer[256] = {0};
  
    while(bytes_to_read > 0) {
      int ret = read(server_socket, print_buffer, bytes_to_read);
      if(ret < 0) {
        perror("read");
        exit(2);
      }
      printf("%s", print_buffer);
      bytes_to_read -= ret;
    }
  }

  printf("Finish reading\n");

  return 0;
}
