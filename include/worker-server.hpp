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

#define PORT_NUMBER 1406
#define WORKER_JOIN -1
#define USER_JOIN -2

/**
 * The server will provide each worker with three types of arguments:
 * 1. the number of inputs
 * 2. the name of the function to execute
 * 3. (at most) one input to the function
 * 4. a chunk of task
 */
typedef struct __attribute__((packed)) task_args_worker {
  int num_args;
  char function_name[256];
  char inputs[256];
  char chunk[256];
}task_arg_worker_t;

typedef struct __attribute__((packed)) task_args_user {
  int num_args;
  char function_name[256];
  char inputs[256];
}task_arg_user_t;
