#ifndef LAB_TEST_COMMON_H
#define LAB_TEST_COMMON_H
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#define CHECK(expr, message) do { if (!(expr)) { \
  fprintf(stderr, "FAIL: %s (%s:%d)\n", message, __FILE__, __LINE__); \
  exit(1); \
} } while (0)
#define P(call) CHECK((call) == 0, #call)
static inline void pause_ms(long ms)
{
  struct timespec t = {ms / 1000, (ms % 1000) * 1000000L};
  while (nanosleep(&t, &t) != 0) {}
}
static inline double cpu_seconds(void)
{
  struct timespec t;
  CHECK(clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t) == 0, "clock_gettime");
  return t.tv_sec + t.tv_nsec / 1e9;
}
static inline void passed(const char *name) { printf("PASS %s\n", name); }
#endif
