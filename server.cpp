#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define D_SERVER_PORT 60519
#define WORKER_JOIN -1
#define USER_JOIN -2

typedef struct worker_thread_args {
  // TODO: Complete this part
  int socket_fd;
}thread_arg_t;

void* worker_thread_fn(void* w) {
  // Duplicate the socket_fd so we can open it twice, once for input and once for output
  thread_arg_t* args = (thread_arg_t*)w;
  int socket_fd = args->socket_fd;
  int socket_fd_copy = dup(socket_fd);
  if(socket_fd_copy == -1) {
    perror("dup failed in client_thread_fn");
    exit(EXIT_FAILURE);
  }

  // Open the socket as a FILE stream so we can use fgets
  FILE* input = fdopen(socket_fd, "r");
  FILE* output = fdopen(socket_fd_copy, "w");
  
  // Check for errors
  if(output == NULL || input == NULL) {
    perror("fdopen failed in client_thread_fn");
    exit(EXIT_FAILURE);
  }

  printf("A worker joined.\n");
  
  // TODO:Here the directory server will give inputs to each worker
}

int main() {
   // Set up a socket
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if(s == -1) {
    perror("socket");
    exit(2);
  }

  // Listen at this address. We'll bind to port 0 to accept any available port
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

  int num_of_workers = 0;
  
  while(true) {
    // Accept a client connection
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);

    // Blocking call: accepts connection from a user/worker and gets its socket
    int client_socket = accept(s, (struct sockaddr*)&client_addr, &client_addr_len);

    // Getting address of the client
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ipstr, INET_ADDRSTRLEN);

    //printf("Client %d connected from %s\n", num_of_workers, ipstr);
    
    // Duplicate the socket_fd so we can open it twice, once for input and once for output
    int client_socket_copy = dup(client_socket);
    if(client_socket_copy == -1) {
      perror("dup failed");
      exit(EXIT_FAILURE);
    }
    
    // Open the socket as a FILE stream so we can use fgets
    FILE* input = fdopen(client_socket, "r");
    FILE* output = fdopen(client_socket_copy, "w");
  
    // Check for errors
    if(input == NULL || output == NULL) {
      perror("fdopen failed");
      exit(EXIT_FAILURE);
    }
  
    // Read lines until we hit the end of the input (the client disconnects)
    char* line = NULL;
    size_t linecap = 100;
    while (getline(&line, &linecap, input) > 0) {
      // Get the integer at the beginning of the message
      char* token = strtok(line, " ");
      int sig = atoi(token);

      // Make argument for the thread function
      thread_arg_t* args = (thread_arg_t*)malloc(sizeof(thread_arg_t));
      args->socket_fd = client_socket;
      
      if(sig == WORKER_JOIN) {
        // Create the thread for worker
        pthread_t thread_worker;
        if(pthread_create(&thread_worker, NULL, worker_thread_fn, args)) {
          perror("pthread_create failed");
          exit(EXIT_FAILURE);
        }
        printf("Client %d connected from %s\n", num_of_workers, ipstr);
        num_of_workers++;
        
      }else if(sig == USER_JOIN) {
        // Create the thread for user
        /* pthread_t thread_user;
        if(pthread_create(&thread_user, NULL, worker_thread_fn, args)) {
          perror("pthread_create failed");
          exit(EXIT_FAILURE);
          }*/
      }
    }
  
    // When we're done, we should free the line from getline
    free(line);
  
    // Print information on the server side
    printf("Worker %d disconnected.\n", num_of_workers);
    
  }
}
