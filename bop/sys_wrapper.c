//Wraps various system functions. Similar design to malloc_wrapper

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include "bop_api.h"

#define GEN_SYS(name, p...)\
  int sys_##name (p){\
    if(!(libc_##name))\
      libc_##name = load_function(#name);\
    return (libc_##name)



void* load_function(char* name){
  void* val = dlsym(RTLD_NEXT, name);
  char * message;
  if((message = dlerror()) != NULL){
    fprintf(stderr, "Couldn't load function %s error %s", name, message);
    abort();
  }
  return val;
}

#define GET_MACRO(_1,_2,_3,_4, NAME,...) NAME
#define GENERATE(...) GET_MACRO(__VA_ARGS__, GENERATE4, GENERATE3)(__VA_ARGS__)

#define GENERATE4(name, fn, argv, envp)\
  static int (*libc_##name)(const char* fn, char * const ag[], char * const enp[]); \
  int name (const char *fn, char *const argv[], char *const envp[]){ \
  if(!(libc_##name)) \
    libc_##name = load_function(#name);\
  cleanup_##name(fn, argv, envp); \
  bop_msg(2, "libc %s is at %p", #name, libc_##name); \
  assert(libc_##name != NULL); \
  return (libc_##name)(fn, argv, envp);}\
  int sys_##name (const char *fn, char *const argv[], char *const envp[]){ \
  if(!(libc_##name)) \
    libc_##name = load_function(#name);\
  bop_msg(2, "libc %s is at %p", #name, libc_##name); \
  assert(libc_##name != NULL); \
  return (libc_##name)(fn, argv, envp);}

#define GENERATE3(name, fn, argv)\
    static int (*libc_##name)(const char*, char *, const x[]); \
    int name (const char *fn, char *const argv[]){ \
    if(!(libc_##name)) \
      libc_##name = load_function(#name);\
    cleanup_##name(fn, argv); \
    bop_msg(2, "libc %s is at %p", #name, libc_##name); \
    assert(libc_##name != NULL); \
    return (libc_##name)(fn, argv);}\
    int sys_##name (const char *fn, char *const argv[]){ \
    if(!(libc_##name)) \
      libc_##name = load_function(#name);\
    bop_msg(2, "libc %s is at %p", #name, libc_##name); \
    assert(libc_##name != NULL); \
    return (libc_##name)(fn, argv);}


GENERATE(execve, filename, a, b);
//GENERATE(execv, fn, argv)

static int (*libc_execv)(const char *filename, char *const argv[]) = NULL;

int execv(const char *filename, char *const argv[]){
  cleanup_execv(filename, argv);
  //should never get here...
  return sys_execv(filename, argv);
}

int sys_execv (const char *filename, char *const argv[]){
  if(libc_execv == NULL){
    libc_execv = load_function("execv");
  }
  bop_msg(2, "libc %s is at %p", "execv", libc_execv);
  assert(libc_execv != NULL);
  return (libc_execv)(filename, argv);
}
