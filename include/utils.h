#ifndef __UTILS_H__
#define __UTILS_H__

#define TRACE_ENABLE
#ifdef TRACE_ENABLE
#include <stdio.h>
#define TRACEF(fmt, ...)                                            \
  printf("(%s@%s:%d): " fmt "\n", __FUNCTION__, __FILE__, __LINE__, \
         ##__VA_ARGS__)
#else
#define TRACEF(fmt, ...)
#endif

#define BEXIT(exp)                        \
  do {                                    \
    if (exp) {                            \
      TRACEF("exp (%s) failed!\n", #exp); \
      goto EXIT;                          \
    }                                     \
  } while (0)

const char* pycrypt_pbin(const char* s, int ns);

#endif
