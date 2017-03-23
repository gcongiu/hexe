#ifndef PREFETCH_INTERN_H
#define PREFETCH_INTERN_H

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
        FETCH_STRIDED = 1,
        WRITE_STRIDED = 2,
        FETCH_LIST = 4,
};

struct prefetch_task{
    char done;
    enum fetch_kind task;
    union {
       struct {
            void *source;
            size_t size;
        }continous;
       struct {
            void *source;
            size_t count;
            size_t block_len;
            size_t stride;
        } strided;
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

struct copy_args {
    size_t size;
    void *to;
    void *from;
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
    struct copy_args *args; 
};
#endif
