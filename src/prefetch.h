#ifndef PREFETCH_H
#define PREFETCH_H

#include <assert.h>
#include <string.h>
#include <sched.h> 
#include "prefetch_intern.h"


inline int lock(uint32_t * log)
{

    uint32_t old = 1;
    do {
        old = __sync_val_compare_and_swap(log, 0, 1);
    } while (old == 1);
    return 0;
}

inline int unlock(uint32_t * log)
{

    uint32_t old = 0;
    old = __sync_val_compare_and_swap(log, 1, 0);
    if (old != 1)
        printf("this should not happen\n");
    return 0;
}
/*
static void copy_worker(void )
{



}*/

static inline int copy_continous(void *src, void *dst, size_t size, struct prefetch_thread *thread)
{

#pragma omp parallel num_threads(thread->n_prefetch_threads)
    {
        int id = omp_get_thread_num();
        int chunk_size = size/thread->n_prefetch_threads;
        int offset = chunk_size *id;
        if(id == thread->n_prefetch_threads-1){	
            chunk_size +=  size%thread->n_prefetch_threads;
        }

        memcpy((char*)dst+offset, (char*)src+offset, chunk_size);

    }

    return 0;
}

static inline int copy_noncontinous(const void *src_start, const void *dst,
                                    const size_t element_size, const int *offsets,
                                    const int elements, volatile int *index)
{

    int i = 0, k = 0;
    uint64_t dest = (uintptr_t) dst;

    char *cp_addr = (char*)src_start;

#pragma omp parallel for
    for (i = 0; i < elements; i++) {
        memcpy((char *) dest, (char *) cp_addr + offsets[i] * element_size, element_size);
        *index = *index + 1;

        dest += element_size;
    }
    return 0;
}

static void *exec_prefetch_thread(void *data)
{

    struct prefetch_thread *thread = (struct prefetch_thread *) data;
    int type;
    struct prefetch_task *task;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &type);
    int i;

    omp_set_dynamic(0);
    omp_set_num_threads(thread->n_prefetch_threads);
    printf("hello \n");
#pragma omp parallel
    {
        int id = omp_get_thread_num();

        printf("I am prefetc prefetchh thread id %d \n", id);
        if(thread->init == 0 && thread->cpusets) {
            hwloc_set_cpubind    (thread->topology, thread->cpusets[id], HWLOC_CPUBIND_THREAD);

        }

#pragma omp master
        thread->init = 1;
    }



    /*   pthread_mutex_lock(&thread->mv);
    */
    while (1) {
        volatile int done = thread->task_list[thread->current_out].done;
        while (!done) {
            task = &thread->task_list[thread->current_out++];
            if (thread->current_out == thread->n_tasks)
                thread->current_out = 0;
            switch (task->task) {
            case (FETCH_CONTIG):
                copy_continous(task->kind.continous.source, task->dest, task->kind.continous.size, thread);
                break;
            case (FETCH_LIST):
                copy_noncontinous(task->kind.list.source_start, task->dest,
                                  task->kind.list.elem_size, task->kind.list.indexes,
                                  task->kind.list.elements, &task->index);
                break;
            default:
                printf("Failure in copy thtrad\n");
            };
            lock(&thread->mp);

 	    _mm_sfence();
            thread->open_tasks--;
            task->done = 1;
            unlock(&thread->mp);

            done = thread->task_list[thread->current_out].done;

        }
    }
}

#ifdef USE_EMU
inline prefetch_handle_t start_prefetch_continous(void *src, void *dst, size_t offset, size_t size, prefetch_thread_t thread) {
#else
inline    prefetch_handle_t start_prefetch_continous(void *src, void *dst, size_t size, prefetch_thread_t thread) {
#endif

        struct prefetch_thread *prefetcher = (struct prefetch_thread *) thread;
        volatile int free;
        struct prefetch_task *task;

       lock(&prefetcher->mp);
       task = &(prefetcher->task_list[prefetcher->current_in++]);
        if(prefetcher->current_in == prefetcher->n_tasks)
            prefetcher->current_in= 0;
        free = task->done;

        if (free == 0 )  {
            unlock(&prefetcher->mp);
            while (free == 0){
                free = task->done;
            }

            lock(&prefetcher->mp);
        }
        task->dest  = dst;
        task->kind.continous.source = src;
        task->kind.continous.size = size;
#ifdef USE_EMU
        task->offset = offset;
#endif
        task->task = FETCH_CONTIG;
        prefetcher->open_tasks++;
 	 _mm_sfence();

        task->done = 0;
        unlock(&prefetcher->mp);
        return task->handle;

 }


#ifdef USE_EMU
inline prefetch_handle_t start_prefetch_noncontinous(void *src_start, void *dst, size_t offset, int* offsets, size_t element_size, int elements,  prefetch_thread_t thread) {
#else
inline prefetch_handle_t start_prefetch_noncontinous(void *src_start, void *dst, int* offsets, size_t element_size, int elements,  prefetch_thread_t thread) {
#endif

    struct prefetch_thread *prefetcher = (struct prefetch_thread *) thread;
    int free;
    lock(&prefetcher->mp);
    struct prefetch_task *task = &(prefetcher->task_list[prefetcher->current_in++]);
    free = task->done;
    if ((!free))  {
        unlock(&prefetcher->mp);
        while (free){
            free = task->done;
        }

        lock(&prefetcher->mp);
    }

    if(prefetcher->current_in == prefetcher->n_tasks)
       prefetcher->current_in= 0;


    task->dest  = dst;
    task->kind.list.source_start = src_start;
    task->kind.list.elem_size = element_size;
    task->kind.list.elements = elements;
    task->kind.list.indexes = offsets;
#ifdef USE_EMU
    task->offset = offset;
#endif
    task->task = FETCH_LIST;

    task->index = -1;
    prefetcher->open_tasks++;
    task->done = 0;
    unlock(&prefetcher->mp);

    return task->handle;


}

inline int prefetch_wakeup(prefetch_thread_t thread){

    struct prefetch_thread *prefetcher = (struct prefetch_thread *) thread;
 //   pthread_mutex_lock(&prefetcher->mv);
    prefetcher->wakeup = 1;
 //   pthread_mutex_unlock(&prefetcher->mv);
   // pthread_cond_signal(&prefetcher->cv);

    return 0;
}

inline int prefetch_allow_sleep(prefetch_thread_t thread) {


    struct prefetch_thread *prefetcher = (struct prefetch_thread *) thread;
  // pthread_mutex_lock(&prefetcher->mv);
    prefetcher->wakeup = 0;
  // pthread_mutex_unlock(&prefetcher->mv);

}


inline int prefetch_wait(prefetch_handle_t handle) {
    struct prefetch_task *task  = (struct prefetch_task*) handle;
    volatile int tmp = task->done;

    while (tmp == 0 ) {
        tmp = task->done;
         _mm_pause();
    }
    return 0;
}

inline int prefetch_wait_index(prefetch_handle_t handle, int index) {
    struct prefetch_task *task  = (struct prefetch_task*) handle;
    volatile int tmp = task->index;

    while (tmp < index) {
        tmp = task->index;

    }
    return 0;
}
inline prefetch_thread_t create_new_thread(int queue_length, int prefetch_threads) {

    struct prefetch_thread *thread;
    struct prefetch_task  *tasks;
    int i;
    thread = (struct prefetch_thread *)malloc(sizeof(struct prefetch_thread));
    assert(thread);
    tasks = (struct prefetch_task*)malloc(sizeof(struct prefetch_task)* queue_length);
    assert(tasks);
    for (i = 0; i< queue_length; i++) {
        tasks[i].done = 1;
        tasks[i].handle = (uint64_t)&tasks[i];
    }
    thread->cpusets = NULL;
    thread->handle = (uint64_t) thread;
    thread->task_list = tasks;
    thread->current_in = 0;
    thread->current_out = 0;
    thread->open_tasks = 0;
    thread->n_tasks = queue_length;
    pthread_cond_init(&thread->cv, NULL);
    pthread_mutex_init(&thread->mv, NULL);
    thread->mp = 0;
    thread->wakeup = 0;
    thread->init = 0;
    thread->n_prefetch_threads = prefetch_threads;

    pthread_create(&thread->thread, NULL,
                         exec_prefetch_thread, (void*)thread);
  //  pthread_detach(thread->thread);
    return thread->handle;
}
inline prefetch_thread_t create_new_thread_with_topo(int queue_length, hwloc_topology_t topology, hwloc_cpuset_t  *cpusets, int n_threads) {

    struct prefetch_thread *thread;
    struct prefetch_task  *tasks;
    int i;
    thread = (struct prefetch_thread *)malloc(sizeof(struct prefetch_thread));
    assert(thread);
    tasks = (struct prefetch_task*)malloc(sizeof(struct prefetch_task)* queue_length);
    assert(tasks);

    for (i = 0; i< queue_length; i++) {
        tasks[i].done = 1;
        tasks[i].handle = (uint64_t)&tasks[i];
    }
    thread->topology = topology;

    thread->cpusets = cpusets;

    thread->handle = (uint64_t) thread;
    thread->task_list = tasks;
    thread->current_in = 0;
    thread->current_out = 0;
    thread->open_tasks = 0;
    thread->n_tasks = queue_length;
    pthread_cond_init(&thread->cv, NULL);
    pthread_mutex_init(&thread->mv, NULL);
    thread->mp = 0;
    thread->wakeup = 0;
    thread->init = 0;


    thread->n_prefetch_threads = n_threads;
    printf("Hello \n");
    pthread_create(&thread->thread, NULL,
                         exec_prefetch_thread, (void*)thread);
  //  pthread_detach(thread->thread);
    return thread->handle;
}


static inline void finish_thread( prefetch_thread_t thread) {
    struct prefetch_thread *prefetcher = (struct prefetch_thread *) thread;
    volatile int out  = prefetcher->open_tasks;
    /*   while(out) {
         out  = prefetcher->open_tasks;
         };
         */
    pthread_cancel(prefetcher->thread);
    free(prefetcher->task_list);
    free(prefetcher);
}

#endif
