#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <errno.h>

typedef int (*real_main_t)(int argc, char** argv);

const char* shared_library = "../shared_library/d-map-injection.so";

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
  return real_main(argc, argv);
}
