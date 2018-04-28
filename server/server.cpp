#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../worker-server.hpp"

#define D_SERVER_PORT 60519
#define WORKER_JOIN -1
#define USER_JOIN -2

typedef struct worker_info {
  int socket_fd;
}worker_info_t;

int num_of_workers;
std::vector<worker_info_t*> list_of_workers; 

// NOTE: Char* don't work here.
typedef struct thread_args {
  int socket_fd;
  char function_name[256];
  int num_args;
  char inputs[256];  // TODO: will change it to a list of inputs in the future
  int section_num;
}thread_arg_t;

/**
void * user_thread_fn (void * u) {
  // Unpack thread arguments
  thread_arg_t* args = (thread_arg_t*)u;
  int socket_fd = args->socket_fd;

  // Duplicate the socket_fd so we can open it twice, once for input and once for output
  int socket_fd_copy = dup(socket_fd);
  if(socket_fd_copy == -1) {
    perror("dup failed in client_thread_fn");
    exit(EXIT_FAILURE);
  }

  // Open the socket as a FILE stream so we can use fgets
  FILE* input = fdopen(socket_fd, "r");
  FILE* output = fdopen(socket_fd_copy, "w");
  if(output == NULL || input == NULL) {
    perror("fdopen failed in client_thread_fn");
    exit(EXIT_FAILURE);
  }

  char * line = NULL;
  size_t linecap = 0;
  while (getline(&line, &linecap, input) > 0) {
    function_name = line;
  }
  close(socket_fd);
  close(socket_fd_copy);
  return NULL;
}
*/

void* worker_thread_fn(void* w) {
  printf("In the worker thread.\n");
  
  // Unpack thread arguments
  thread_arg_t* args = (thread_arg_t*)w;
  int socket_fd = args->socket_fd;
  char* function_name = args->function_name;
  int num_args = args->num_args;
  int section_num = args->section_num;
  char* inputs = args->inputs;
  
  // Sends the function name to the worker with its section number as string
  // TODO: These will eventually be done in the user thread
  task_arg_t* task_arg = (task_arg_t*)malloc(sizeof(task_arg_t));
  task_arg->num_args = num_args;
  strcpy(task_arg->function_name, function_name);
  task_arg->section_num = section_num;
  strcpy(task_arg->inputs, inputs); // TODO: will change to a list of inputs in the future
  write(socket_fd, (void*)task_arg, sizeof(task_arg_t));

  // Read cracked passwords from the worker and print it to the console
  char buffer[256];
  int bytes_read = read(socket_fd, buffer, 256);
  if(bytes_read < 0) {
    perror("read failed");
    exit(2);
  }
  char* token = strtok(buffer, "\n");
  while (token != NULL) {
    printf("%s\n", token);
    token = strtok(NULL, "\n");
  }
  
  // Close the socket
  close(socket_fd);

  return NULL;
}

int main() {
  // Set up a socket
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if(s == -1) {
    perror("socket");
    exit(2);
  }

  // Listen for any address on port 60519.
  struct sockaddr_in addr;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(D_SERVER_PORT);

  // Bind to the specified address
  if(bind(s, (struct sockaddr*)&addr, sizeof(struct sockaddr_in))) {
    perror("bind");
    exit(2);
  }

  // Become a server socket
  listen(s, 2);

  // Initialize the number of workers
  int num_of_workers = 0;

  // Repeatedly accept client connections
  while(true) {
    // Accept a client connection
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);

    // Blocking call: accepts connection from a user/worker and gets its socket
    int client_socket = accept(s, (struct sockaddr*)&client_addr, &client_addr_len);

    // Getting IP address of the client
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ipstr, INET_ADDRSTRLEN);

    char buffer[256];
    int bytes_read = read(client_socket, buffer, 256);
    if(bytes_read < 0) {
      perror("read failed");
      exit(2);
    }
    // Get the integer at the beginning of the message
    char* token = strtok(buffer, "\n");
    int sig = atoi(token);

    // Checks if the client is a user or a worker
    if(sig == WORKER_JOIN) {
      // Create the thread for worker
      thread_arg_t* args = (thread_arg_t*)malloc(sizeof(thread_arg_t));
      args->socket_fd = client_socket;
      strcpy(args->function_name, "entrance");
      args->num_args = 3;
      args->section_num = num_of_workers;
      strcpy(args->inputs, "/home/qishuyi/213/final_project/D-Map/tests/passwords.txt");  // TODO: will change to a list of inputs in the future

      pthread_t thread_worker;
      if(pthread_create(&thread_worker, NULL, worker_thread_fn, args)) {
        perror("pthread_create failed");
        exit(EXIT_FAILURE);
      }
      printf("Client %d connected from %s\n", num_of_workers, ipstr);
      num_of_workers++;

      // Add worker to the list
      worker_info_t* info = (worker_info_t*)malloc(sizeof(worker_info_t));
      info->socket_fd = client_socket;
      list_of_workers.push_back(info);
    }/** else if(sig == USER_JOIN) {
      // Create the thread for user
      thread_arg_t* args = (thread_arg_t*)malloc(sizeof(thread_arg_t));
      args->socket_fd = client_socket;
      args->index = 0;
        
      pthread_t thread_user;
      if(pthread_create(&thread_user, NULL, user_thread_fn, args)) {
      perror("pthread_create failed");
      exit(EXIT_FAILURE);
      }
      }*/
  
    // Print information on the server side
    printf("Worker %d disconnected.\n", num_of_workers);
    
  }
}
