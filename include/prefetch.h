#ifndef PREFETCH_H
#define PREFETCH_H

#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <hwloc.h>
#define N_THREADS 32

typedef  uint64_t prefetch_handle_t;
typedef  uint64_t prefetch_thread_t;

enum fetch_kind {
        FETCH_CONTIG = 0,
        FETCH_LIST =1,
};

struct prefetch_task{
    char done;
    enum fetch_kind task;
    union {
       struct {
            void *source;
            int size;
        }continous;
        struct {
            int* indexes;
            int elem_size;
            int elements;
            void* source_start;
        }list;
    }kind;
    void *dest;
#ifdef USE_EMU
    size_t offset;
#endif
    volatile int index;
    prefetch_handle_t handle;
};

struct prefetch_thread{
    struct prefetch_task *task_list;
    int current_in;
    int current_out;
    int n_tasks;
    int open_tasks;
    int init;
    prefetch_thread_t handle;
    pthread_t thread;
    pthread_cond_t cv;
    pthread_mutex_t mv;
    uint16_t wakeup;
    uint32_t mp;
    hwloc_cpuset_t *cpusets;
    hwloc_topology_t topology;
    hwloc_obj_t *roots;
    unsigned int nroots;
    int n_prefetch_threads;
};

void finish_thread( prefetch_thread_t thread);

prefetch_thread_t create_new_thread(int queue_length, int prefetch_threads);
prefetch_thread_t create_new_thread_with_topo(int queue_length, hwloc_topology_t topology, hwloc_cpuset_t  *cpusets, int n_threads);

int prefetch_wait(prefetch_handle_t handle);
int  prefetch_wait_index(prefetch_handle_t handle, int index);
#ifdef USE_EMU
    prefetch_handle_t start_prefetch_continous(void *src, void *dst, size_t offset, size_t size, prefetch_thread_t thread);
   prefetch_handle_t start_prefetch_noncontinous(void *src_start, void *dst, size_t offset, int* offsets, size_t element_size, int elements,  prefetch_thread_t thread);
#else
    prefetch_handle_t start_prefetch_continous(void *src, void *dst, size_t size, prefetch_thread_t thread);
    prefetch_handle_t start_prefetch_noncontinous(void *src_start, void *dst, int* offset_list ,size_t element_size, int elements,  prefetch_thread_t thread);
#endif
int prefetch_wakeup(prefetch_thread_t thread);
int prefetch_allow_sleep(prefetch_thread_t thread);
#endif
