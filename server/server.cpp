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

//check if there is a user
bool user_exist = false;
int user_socket;

int num_of_workers;
std::vector<int> list_of_workers; 

// NOTE: Char* don't work here.
/**
typedef struct thread_args {
  int socket_fd;
  char function_name[256];
  int num_args;
  char inputs[256];  // TODO: will change it to a list of inputs in the future
  int section_num;
}worker_thread_arg_t;
*/

typedef struct thread_args {
  int socket_fd;
}thread_arg_t;


void * user_thread_fn (void* u) {
  // Unpack thread arguments
  thread_arg_t* args = (thread_arg_t*)u;
  int socket_fd = args->socket_fd;

  task_arg_user_t* task_arg_user = (task_arg_user_t*)malloc(sizeof(task_arg_user_t));
  read(socket_fd, (void*)task_arg_user, sizeof(task_arg_user_t));

  // Unpack arguments from the user
  int num_args = task_arg_user->num_args;
  char* function_name;
  strcpy(function_name, task_arg_user->function_name);
  char* inputs;
  strcpy(inputs, task_arg_user->inputs); // TODO: will change to a list of inputs in the future

  std::vector<int>::iterator iter;
  int section_num = 0;

  // Iterate through each worker in list_of_workers
  for(iter = list_of_workers.begin(); iter != list_of_workers.end(); iter++) {
    // Pack arguments for the worker
    task_arg_worker_t* task_arg_worker = (task_arg_worker_t*)malloc(sizeof(task_arg_worker_t));
    task_arg_worker->num_args = num_args;
    strcpy(task_arg_worker->function_name, function_name);
    strcpy(task_arg_worker->inputs, inputs);
    task_arg_worker->section_num = section_num;

    // Send the task_arg_worker_t to the worker
    int worker_socket = *iter;
    write(worker_socket, (void*)task_arg_worker, sizeof(task_arg_worker_t));
    
    section_num++;
  }
  
  close(socket_fd);
  return NULL;
}


void* worker_thread_fn(void* w) {
  printf("In the worker thread.\n");
  
  // Unpack thread arguments
  thread_arg_t* args = (thread_arg_t*)w;
  int socket_fd = args->socket_fd;

  // Read cracked passwords from the worker and print it to the console
  char buffer[256];
  int bytes_read = read(socket_fd, buffer, 256);
  if(bytes_read < 0) {
    perror("read failed");
    exit(2);
  }
  while ((bytes_read = read(socket_fd, buffer, 256)) > 0) {
    perror("read failed");
    exit(2);
  }

  // Swap the server_socket in and use it as stdout 
  if(dup2(user_socket, STDOUT_FILENO) == -1) {
    fprintf(stderr, "Failed to set user socket as output\n");
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
      /**
      strcpy(args->function_name, "entrance");
      args->num_args = 3;
      args->section_num = num_of_workers;
      strcpy(args->inputs, "/home/qishuyi/213/final_project/D-Map/tests/passwords.txt");  // TODO: will change to a list of inputs in the future
      */

      pthread_t thread_worker;
      if(pthread_create(&thread_worker, NULL, worker_thread_fn, args)) {
        perror("pthread_create failed");
        exit(EXIT_FAILURE);
      }
      printf("Client %d connected from %s\n", num_of_workers, ipstr);

      // Add worker to the list
      list_of_workers.push_back(client_socket);
      num_of_workers++;
    } else if(sig == USER_JOIN) {
      if(!user_exist){
        // Create the thread for user
        thread_arg_t* args = (thread_arg_t*)malloc(sizeof(thread_arg_t));
        args->socket_fd = client_socket;

        // Let the server know the user socket
        user_socket = client_socket;
        
        pthread_t thread_user;
        if(pthread_create(&thread_user, NULL, user_thread_fn, args)) {
          perror("pthread_create failed");
          exit(EXIT_FAILURE);
        }
      }else{
        char* message = (char*)"Server is busy.\n";
        write(client_socket, message, strlen(message));
      }
   }
  
    // Print information on the server side
    printf("Worker %d disconnected.\n", num_of_workers);
    
  }
}
