//Wraps various system functions. Similar design to malloc_wrapper
#ifdef EXEC_ON_MONITOR
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include "bop_api.h"

void* load_function(char* name){
  void* val = dlsym(RTLD_NEXT, name);
  char * message;
  if((message = dlerror()) != NULL){
    fprintf(stderr, "Couldn't load function %s error %s", name, message);
    abort();
  }
  return val;
}

int execve(const char *filename, char *const argv[], char *const envp[]){
  bop_msg(3, "caught execve call for %s", filename);
  cleanup_execve(filename, argv, envp);
  //should never get here...
  bop_msg(1, "cleanup_execve returned!");
  return sys_execv(filename, argv, envp);
}

int sys_execve (const char *filename, char *const argv[]){
  static int (*libc_execve)(const char *filename, char *const argv[], char *const envp[]);
  if(libc_execv == NULL){
    libc_execv = load_function("execv");
  }
  bop_msg(2, "libc %s is at %p", "execv", libc_execv);
  assert(libc_execv != NULL);
  return (libc_execv)(filename, argv);
}




int execv(const char *filename, char *const argv[]){
  bop_msg(3, "caught execv call for %s", filename);
  cleanup_execv(filename, argv);
  //should never get here...
  bop_msg(1, "cleanup_execve returned!");
  return sys_execv(filename, argv);
}

int sys_execv (const char *filename, char *const argv[]){
  static int (*libc_execv)(const char *filename, char *const argv[]);
  if(libc_execv == NULL){
    libc_execv = load_function("execv");
  }
  bop_msg(2, "libc %s is at %p", "execv", libc_execv);
  assert(libc_execv != NULL);
  return (libc_execv)(filename, argv);
}

#endif
