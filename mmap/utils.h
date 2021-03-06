#ifndef _INCLUDE_UTILS_H_
#define _INCLUDE_UTILS_H_

#include <unistd.h>

typedef enum {
  UNKNOWN = 0,
  BITS32 = 32,
  BITS64 = 64
} bit_type;

int generic_fault(long ret);
bit_type get_type(pid_t pid);
void die(char *str);

#endif
