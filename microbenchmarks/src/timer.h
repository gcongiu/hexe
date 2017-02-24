#include <sys/time.h>

#include <stdint.h>
static inline uint64_t ucs_arch_read_hres_clock()
{
    uint32_t low, high;
    asm volatile ("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64_t)high << 32) | (uint64_t)low;
}
struct aclock{

 uint64_t start;
 uint64_t stop;
 unsigned long long elapsed;
};

static inline int init_aclock(struct aclock* a_clock)
{
   a_clock->start = 0;
   a_clock->stop = 0;
   return 0;
}

static inline int start_aclock(struct aclock* a_clock)
{

  a_clock->start = ucs_arch_read_hres_clock();
  return 0;
}
static inline int end_aclock(struct aclock* a_clock)
{

  a_clock->stop = ucs_arch_read_hres_clock();
  a_clock->elapsed = a_clock->stop - a_clock->start;
  return 0;
}

static inline double get_seconds(struct aclock* a_clock)
{

  return a_clock->elapsed/1e9;

}
static inline unsigned long long get_useconds(struct aclock* a_clock)
{

  return a_clock->elapsed/1e3;

}
