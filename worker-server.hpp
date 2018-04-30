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

#define D_SERVER_PORT 60519
#define WORKER_JOIN -1
#define USER_JOIN -2

/**
 * The server will provide each worker with three types of arguments:
 * 1. the name of the function to execute
 * 2. a list of inputs
 * 3. the worker's section number (varies from worker to worker)
 */
typedef struct task_args_worker {
  int num_args;
  char function_name[256];
  char inputs[256];
  // TODO: will change it to a list of inputs in the future
  // std::vector<char*> inputs;
  int section_num;
}task_arg_worker_t;

typedef struct task_args_user {
  int num_args;
  char function_name[256];
  char inputs[256];
  // TODO: will change it to a list of inputs in the future
  // std::vector<char*> inputs;
}task_arg_user_t;
