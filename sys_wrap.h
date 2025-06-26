#ifndef SYS_WRAP_H
#define SYS_WRAP_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define CHECK_CALL_RET1(call)                                                        \
  do {                                                                         \
    if ((call) < 0) {                                                          \
      fprintf(stderr, "[%s -> %s] %s failed: %s\n", __FILE__, __func__, #call, \
              strerror(errno));                                                \
      return 1;                                                                \
    }                                                                          \
  } while (0)



#endif // SYS_WRAP_H
