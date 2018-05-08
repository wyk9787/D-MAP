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
#include <unordered_map>
#include <algorithm>
#include "worker-server.hpp"

// define function pointers 
using has_next_t = bool  (*)(void);
using get_next_t = char* (*)(void);

// check if there is a user
bool user_exist = false;
int user_socket;

int num_of_workers;
std::unordered_map<int, bool> list_of_workers;

typedef struct thread_args {
  int socket_fd;
}thread_arg_t;

void * user_thread_fn (void* u) {
  printf("In the user thread.\n");
  // Unpack thread arguments
  thread_arg_t* args = (thread_arg_t*)u;
  int socket_fd = args->socket_fd;

  // 1.Read the size of the executable file first. The number will be sent as a 10-byte string.
  char executable_size[10];
  if((read(socket_fd, (void*)executable_size, 10)) == -1) {
    perror("read");
    exit(1);
  }
  printf("Executable size is: %s\n", executable_size);
  long filesize = strtol(executable_size, NULL, 10);
  printf("Read size: %ld.\n", filesize);

  // 2.Then get the executable file from the user (this file should be of filesize).
  char executable[filesize];
  int bytes_to_read = filesize;
  int prev_read = 0;

  // Open a temp file in the "write-binary" mode.
  FILE * exe_lib = fopen("temp.so", "wb");
  if(exe_lib == NULL) { 
    perror("Failed: ");
    exit(1);
  }

  // Keep reading bytes until the entire file is read.
  while(bytes_to_read > 0){
    // Executable_read indicates the bytes already read by the read function.
    int executable_read = read(socket_fd, executable + prev_read, bytes_to_read);
    printf("Read %d bytes of executable.\n", executable_read);
    bytes_to_read -= executable_read;
    prev_read += executable_read;
  }

  // Write the read bytes to the file.
  if (fwrite(executable, filesize, 1, exe_lib) != 1){
    fprintf(stderr, "fwrite\n");
    exit(1);
  }

  // Close file stream.
  fclose(exe_lib);
  
  // 3.Finally get the arguments from user.
  task_arg_user_t task_args;
  int bytes = read(socket_fd, &task_args, sizeof(task_arg_user_t));
  if(bytes != sizeof(task_arg_user_t)) {
    fprintf(stderr, "Read: Not reading enough bytes. Expected: %lu; Actual: %d.\n", sizeof(task_arg_user_t), bytes);
    exit(2);
  }
  
  // Unpack arguments from the user
  int num_args = task_args.num_args;
  char function_name[256];
  strcpy(function_name, task_args.function_name);
  char inputs[256];
  strcpy(inputs, task_args.inputs); // TODO: will change to a list of inputs in the future
  printf("Read num_of_arguments: %d, read functionname: %s, read inputs: %s.\n", num_args, function_name, inputs);

  // Load the shared library
  void* program = dlopen("temp.so", RTLD_LAZY | RTLD_GLOBAL);
  if(program == NULL) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    exit(EXIT_FAILURE);
  }
  // Get the iterator functions
  dlerror();
  has_next_t has_next = (has_next_t)dlsym(program, "has_next");
  char* error = dlerror();
  if(error != NULL) {
    printf("has_next error: %s\n", error);
    exit(1);
  }

  dlerror();
  get_next_t get_next = (get_next_t)dlsym(program, "get_next");
  if(error != NULL) {
    printf("get_next error: %s\n", error);
    exit(1);
  }
  
  int section_num = 0;
  int list_size = list_of_workers.size();
  
  while(has_next()) {
    printf("Inside of has_next\n");
    int worker_socket;
    // Check if the worker is free. If so, give it a task; otherwise, skip it.
    bool found = false;
    while(true) {
      for(auto cur : list_of_workers) {
        if(cur.second) { // This worker is available
          list_of_workers[cur.first] = false;
          worker_socket = cur.first;
          found = true;
          break;
        }
      }
      if(found) break; // If we found an avaialble worker, break from the while loop
    }
    
    //First send the size of the executable file
    write(worker_socket, (void*)executable_size, 10);
    
    // Send the executable file to the worker
    write(worker_socket, (void*)executable, filesize);
    
    // Pack arguments for the worker
    task_arg_worker_t* task_arg_worker = (task_arg_worker_t*)malloc(sizeof(task_arg_worker_t));
    task_arg_worker->num_args = num_args;
    strcpy(task_arg_worker->function_name, function_name);
    strcpy(task_arg_worker->inputs, inputs);
    strcpy(task_arg_worker->chunk, get_next());

    // Send the task_arg_worker_t to the worker
    write(worker_socket, (void*)task_arg_worker, sizeof(task_arg_worker_t));
  }

  bool check = false;
  while(check == false) {
    for(auto cur : list_of_workers) {
      if(cur.second == false) { // This worker is not done
        goto cnt;
      }
    }
    printf("now I'm done\n");
    check = true;
  cnt:;
  }
  
  char buffer[10] = "0";
  if (write(user_socket, buffer, 10) < 0) {
    perror("write");
    exit(2);
  }

  if (close(socket_fd) < 0){
    perror("Close in user thread");
    exit(2);
  } 
  printf("Got all the done workers.\n");
  return NULL;
}


void* worker_thread_fn(void* w) {
  printf("In the worker thread.\n");
  
  // Unpack thread arguments
  thread_arg_t* args = (thread_arg_t*)w;
  int worker_socket = args->socket_fd;

  while(true) {  
    // Read cracked passwords from the worker and print it to the console
    char buffer[10];
    // Read output size from the worker
    int bytes_read = read(worker_socket, buffer, 10);
    // Save the size of the output
    int bytes_to_read = atoi(buffer);
    if (bytes_to_read == 0) {
      list_of_workers[worker_socket] = true;
      continue;
    }
    
    // Write output size to the user
    if(write(user_socket, buffer, 10) < 0) {
      perror("write");
      exit(2);
    }

    char output_buffer[256] = {0};

    while(bytes_to_read > 0) {

      // Read actual output from the worker
      int output_read = read(worker_socket, output_buffer, bytes_to_read);
      // Check for error
      if(output_read < 0) {
        perror("read failed");
        exit(2);
      }
      output_buffer[output_read] = '\0';

      printf("Received: %d bytes: %s", output_read, output_buffer);

      // Write output to the user
      if(write(user_socket, output_buffer, output_read) < 0) {
        perror("write");
        exit(2);
      }
      bytes_to_read -= output_read;
    }
    list_of_workers[worker_socket] = true;
  }
  
  // Close the socket
  if (close(worker_socket) < 0){
    perror("Close in worker thread");
    exit(2);
  }

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
      list_of_workers.insert({client_socket, true});
      num_of_workers++;
    } else if(sig == USER_JOIN) {
      if(!user_exist){
        printf("Identified a user.\n");
        // Create the thread for user
        thread_arg_t* args = (thread_arg_t*)malloc(sizeof(thread_arg_t));
        args->socket_fd = client_socket;

        // Let the server know the user socket
        user_socket = client_socket;
        printf("In main, user_socket: %d\n", user_socket);
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
