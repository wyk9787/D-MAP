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

//check if there is a user
bool user_exist = false;
int user_socket;

int num_of_workers;
std::vector<int> list_of_workers; 

typedef struct thread_args {
  int socket_fd;
}thread_arg_t;

void * user_thread_fn (void* u) {
  printf("In the user thread.\n");
  // Unpack thread arguments
  thread_arg_t* args = (thread_arg_t*)u;
  int socket_fd = args->socket_fd;

  // Read the size of the executable file first
  char executable_size[10];
  if((read(socket_fd, (void*)executable_size, 10)) == -1) {
    perror("read");
    exit(1);
    
  }
  printf("Executable size is: %s\n", executable_size);
  long filesize = strtol(executable_size, NULL, 10);
  printf("Read size: %ld.\n", filesize);

  // Then get the executable file from the user (this file should be of filesize).
  char executable[filesize];
  int bytes_to_read = filesize;
  FILE * exe_lib = fopen("temp.so", "wb");
  int prev_written = 0;
  while(bytes_to_read > 0){
    if (exe_lib == NULL) { 
      perror("Failed: ");
      exit(1);
    }
    int executable_read = read(socket_fd, executable + prev_written, bytes_to_read);
    printf("Read %d bytes of executable.\n", executable_read);
    //printf("Write %d bytes of executable.\n", bytes_written);
    bytes_to_read -= executable_read;
    prev_written += executable_read;
  }
  int items_written = fwrite(executable, filesize, 1, exe_lib);
  if (items_written != 1){
      
    fprintf(stderr, "fwrite\n");
    exit(1);
  }
  fclose(exe_lib);
  printf("Read executable\n");
  
  // Finally get the arguments from user
  task_arg_user_t task_args;
  int bytes = read(socket_fd, &task_args, sizeof(task_arg_user_t));
  if(bytes == -1) {
    perror("read");
    exit(2);
  }
  printf("Bytes read: %d\n", bytes);
  
  printf("Read arguments.\n");
  // Unpack arguments from the user
  int num_args = task_args.num_args;
  char function_name[256];
  strcpy(function_name, task_args.function_name);
  char inputs[256];
  strcpy(inputs, task_args.inputs); // TODO: will change to a list of inputs in the future
  printf("Read num_of_arguments: %d, read functionname: %s, read inputs: %s.\n", num_args, function_name, inputs);
  
  std::vector<int>::iterator iter;
  int section_num = 0;

  // Iterate through each worker in list_of_workers
  for(iter = list_of_workers.begin(); iter != list_of_workers.end(); iter++) {
    int worker_socket = *iter;

    //First send the size of the executable file
    write(worker_socket, (void*)executable_size, 10);
    
    // Send the executable file to the worker
    write(worker_socket, (void*)executable, filesize);
    
    // Pack arguments for the worker
    task_arg_worker_t* task_arg_worker = (task_arg_worker_t*)malloc(sizeof(task_arg_worker_t));
    task_arg_worker->num_args = num_args;
    strcpy(task_arg_worker->function_name, function_name);
    strcpy(task_arg_worker->inputs, inputs);
    task_arg_worker->section_num = section_num;

    // Send the task_arg_worker_t to the worker
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
  int worker_socket = args->socket_fd;

  // Read cracked passwords from the worker and print it to the console
  char buffer[256];
  int bytes_read;

  // Swap the user_socket in and use it as stdout 
  if(dup2(user_socket, STDOUT_FILENO) == -1) {
    fprintf(stderr, "Failed to set user socket as output\n");
    exit(2);
  }

  // Continuously reading from the worker_socket to get output
  while ((bytes_read = read(worker_socket, buffer, 256)) > 0) {
    char* token = strtok(buffer, "\n");
    while (token != NULL) {
      printf("%s\n", token);
      token = strtok(NULL, "\n");
    }
  }

  // Check for error
  if(bytes_read < 0) {
    perror("read failed");
    exit(2);
  }
  
  // Close the socket
  close(worker_socket);

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

    printf("Accept a connection from client.\n");
    // Getting IP address of the client
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ipstr, INET_ADDRSTRLEN);

    char buffer[256];
    int bytes_read = read(client_socket, buffer, 256);
    if(bytes_read < 0) {
      perror("read failed");
      exit(2);
    }
    printf("Read the identification message.\n");
    // Get the integer at the beginning of the message
    char* token = strtok(buffer, "\n");
    int sig = atoi(token);

    printf("Got message from client.\n");
    // Checks if the client is a user or a worker
    if(sig == WORKER_JOIN) {
      // Create the thread for worker
      thread_arg_t* args = (thread_arg_t*)malloc(sizeof(thread_arg_t));
      args->socket_fd = client_socket;

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
        printf("Identified a user.\n");
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
