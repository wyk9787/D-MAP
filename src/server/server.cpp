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
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
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

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void * user_thread_fn (void* u) {
  // Unpack thread arguments
  thread_arg_t* args = (thread_arg_t*)u;
  int socket_fd = args->socket_fd;

  // 1.Read the size of the executable file first. The number will be sent as a 10-byte string.
  char executable_size[10];
  if((read(socket_fd, (void*)executable_size, 10)) == -1) {
    perror("read");
    return NULL;
  }
  long filesize = strtol(executable_size, NULL, 10);

  // 2.Then get the executable file from the user (this file should be of filesize).
  char executable[filesize];
  int bytes_to_read = filesize;
  int prev_read = 0;

  // Keep reading bytes until the entire file is read.
  while(bytes_to_read > 0){
    // Executable_read indicates the bytes already read by the read function.
    int executable_read = read(socket_fd, executable + prev_read, bytes_to_read);
    if(executable_read < 0) {
      perror("read executable");
      return NULL;
    }
    bytes_to_read -= executable_read;
    prev_read += executable_read;
  }

  // Set up the filename for shared library
  char shared_library[40];
  int rand_num = rand();
  sprintf(shared_library, "./temp%d.so", rand_num);
  
  // Open a temp file in the "write-binary" mode.
  FILE * exe_lib = fopen(shared_library, "wb");
  if(exe_lib == NULL) { 
    perror("Failed: ");
    exit(1);
  }
  
  // Write the read bytes to the file.
  if (fwrite(executable, filesize, 1, exe_lib) != 1){
    fprintf(stderr, "fwrite\n");
    exit(1);
  }

  // Close file stream.
  fclose(exe_lib);

  // Make shared object executable
  if(chmod(shared_library, S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IRGRP | S_IWGRP | S_IXOTH | S_IROTH | S_IWOTH) != 0) {
    perror("chmod");
    exit(2);
  }
  
  // 3.Finally get the arguments from user.
  task_arg_user_t task_args;
  int bytes = read(socket_fd, &task_args, sizeof(task_arg_user_t));
  if(bytes != sizeof(task_arg_user_t)) {
    fprintf(stderr, "Read: Not reading enough bytes. Expected: %lu; Actual: %d.\n", sizeof(task_arg_user_t), bytes);
    return NULL;
  }
  
  
  // Unpack arguments from the user
  int num_args = task_args.num_args;
  char function_name[256];
  strcpy(function_name, task_args.function_name);
  char inputs[256];
  strcpy(inputs, task_args.inputs); // TODO: will change to a list of inputs in the future

  // Load the shared library
  dlerror();
  void* program = dlopen(shared_library, RTLD_LAZY | RTLD_GLOBAL);
  if(program == NULL) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    exit(EXIT_FAILURE);
  }

  // Get the iterator functions
  dlerror();
  has_next_t has_next = (has_next_t)dlsym(program, "has_next");
  char* error = dlerror();
  if(error != NULL) {
    fprintf(stderr, "has_next error: %s\n", error);
    exit(1);
  }

  dlerror();
  get_next_t get_next = (get_next_t)dlsym(program, "get_next");
  if(error != NULL) {
    fprintf(stderr, "get_next error: %s\n", error);
    exit(1);
  }

  // Loop through all workers
  for(auto cur : list_of_workers) {
    int socket = cur.first;
    char temp[filesize];
    memcpy(temp, executable, filesize);
    
    //First send the size of the executable file
    if(write(socket, executable_size, 10) != 10) {
      perror("write");
      exit(2);
    }

    // Send the executable file to the worker
    if(write(socket, temp, filesize) != filesize) {
      perror("Write executable");
      exit(2);
    }
  }

  // Pack arguments for the worker
  task_arg_worker_t* task_arg_worker = (task_arg_worker_t*)malloc(sizeof(task_arg_worker_t));
  task_arg_worker->num_args = num_args;
  strcpy(task_arg_worker->function_name, function_name);
  strcpy(task_arg_worker->inputs, inputs);
  
  while(has_next()) {
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

    // Copy the new chunk into the struct
    strcpy(task_arg_worker->chunk, get_next());

    // Set up the continue command
    char to_go[2];
    sprintf(to_go, "%d", 1);

    // Tell the worker there is more work on this task
    if(write(worker_socket, to_go, 2) < 0) {
      perror("write");
      exit(2);
    }
    
    // Send the task_arg_worker_t to the worker
    if(write(worker_socket, (void*)task_arg_worker, sizeof(task_arg_worker_t)) < 0) {
      perror("write");
      exit(2);
    }
  }

  // Wait until all workers done with their tasks
  while(!std::all_of(list_of_workers.begin(), list_of_workers.end(), [](std::pair<int, bool> cur){ return cur.second == true; }));

  // Set up the finishing message
  char to_go[2];
  sprintf(to_go, "%d", 0);
   
  for(auto cur : list_of_workers) {
    int socket = cur.first;
    // Tell the worker there is no more work on this task
    if(write(socket, to_go, 2) < 0) {
      perror("write");
      exit(2);
    }
  }

  // Close the socket to the user so user know the task is finished
  if (close(socket_fd) < 0){
    perror("Close in user thread");
    exit(2);
  }

  if(dlclose(program) != 0) {
    dlerror();
    exit(1);
  }

  printf("Got all the done workers.\n");
  
  return NULL;
}


void* worker_thread_fn(void* w) {
  // Unpack thread arguments
  thread_arg_t* args = (thread_arg_t*)w;
  int worker_socket = args->socket_fd;

  while(true) {  
    char buffer[10];
    // Read output size from the worker
    int bytes_read = read(worker_socket, buffer, 10);
    
    // Save the size of the output
    int bytes_to_read = atoi(buffer);
    
    //not sending to the user if chunk is size 0
    if (bytes_to_read == 0) {
      list_of_workers[worker_socket] = true;
      continue;
    }

    pthread_mutex_lock(&mutex);
    // Write output size to the user
    if(write(user_socket, buffer, 10) < 0) {
      perror("write");
      return NULL;
    }
  
    char output_buffer[256] = {0};
    int prev_read = 0;
    
    while(bytes_to_read > 0) {
      // Read actual output from the worker
      int output_read = read(worker_socket, output_buffer + prev_read, bytes_to_read);
      if(output_read < 0) {
        perror("read failed");
        exit(2);
      }
      if (output_read == 0)
        break;
      output_buffer[output_read] = '\0';

      // Write output to the user
      if(write(user_socket, output_buffer, output_read) < 0) {
        perror("write");
        return NULL;
      }
      bytes_to_read -= output_read;
      prev_read += output_read;
    }
    list_of_workers[worker_socket] = true;
    pthread_mutex_unlock(&mutex);
  }
  
  // Close the socket
  if (close(worker_socket) < 0){
    perror("Close in worker thread");
    exit(2);
  }

  return NULL;
}

int main() {
  srand(time(NULL));
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
  addr.sin_port = htons(PORT_NUMBER);

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

    printf("Accept a connection.\n");
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

      pthread_t thread_worker;
      if(pthread_create(&thread_worker, NULL, worker_thread_fn, args)) {
        perror("pthread_create failed");
        exit(EXIT_FAILURE);
      }
      printf("Worker %d connected from %s\n", num_of_workers, ipstr);

      // Add worker to the list
      list_of_workers.insert({client_socket, true});
      num_of_workers++;
    } else if(sig == USER_JOIN) {
      if(!user_exist){
        printf("A user joined.\n");
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
  }
}
