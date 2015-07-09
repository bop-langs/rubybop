//Wraps various system functions. Similar design to malloc_wrapper

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include "bop_api.h"

static int (*libc_execve)(const char *filename, char *const argv[], char *const envp[]);
static int (*libc_execv)(const char *path, char *const argv[]);

void* load_function(char* name){
  void* val = dlsym(RTLD_NEXT, name);
  char * message;
  if((message = dlerror()) != NULL){
  //  fprintf(stderr, "Couldn't load function %s error %s", name, message);
    abort();
  }
  return val;
}
int execve(const char *filename, char *const argv[], char *const envp[]){
//  printf("\t\tcaught execve\n");
  if(libc_execve == NULL)
    libc_execve = load_function("execve");

  exec_cleanup();
  return libc_execve(filename, argv, envp);
}
int execv(const char *filename, char *const argv[]){
//  printf("\t\tcaught execv\n");
  if(libc_execv == NULL)
    libc_execv = load_function("execv");
  exec_cleanup();
  return libc_execv(filename, argv);
}
