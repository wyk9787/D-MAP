#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

typedef int (*real_main_t)(int argc, char** argv);

const char* shared_library = "../worker/injection.so";
const char* test_file = "test.txt";

int main(int argc, char** argv) {
  errno = 0;
  void* injection = dlopen(shared_library, RTLD_LAZY | RTLD_GLOBAL);
  if(injection == NULL) {
    perror("dlopen");
    exit(1);
  }
  dlerror();
  real_main_t real_main = (real_main_t)dlsym(injection, "entrance");
  char* error = dlerror();
  if(error != NULL) {
    printf("Error: %s\n", error);
    exit(1);
  }
 
  // Open the test file
  // TODO: When we actually use this, the socket that connects the server is
  // this new_output
  int new_output = open(test_file, O_WRONLY);
  if(new_output == -1) {
    fprintf(stderr, "Failed to open output file %s\n", test_file);
    exit(1);
  }

  // Now swap this file in and use it as stdout 
  if(dup2(new_output, STDOUT_FILENO) == -1) {
    fprintf(stderr, "Failed to set new file as output\n");
    exit(2);
  } 

  printf("Write to file test!\n");

  return real_main(argc, argv);
}
