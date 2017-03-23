#ifndef PREFETCH_H
#define PREFETCH_H

#include <assert.h>
#include <string.h>
#include <sched.h> 
#include "prefetch_intern.h"

int count = 0;

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

struct prefetch_thread *pre_thread;

static inline int copy_continous(void *src, void *dst, size_t size)
{
    count ++;
#pragma omp parallel
    {
        int id = omp_get_thread_num();
        int chunk_size = size/pre_thread->n_prefetch_threads;
        int offset = chunk_size *id;
        if(id == pre_thread->n_prefetch_threads-1){	
            chunk_size +=  size%pre_thread->n_prefetch_threads;
        }
/*        int i;
         struct timeval tv;
        time_t f1,f2;
        time_t delta = 0;
            gettimeofday(&tv, NULL); 
            f1 = tv.tv_usec;
*/
        memcpy((char*)dst+offset, (char*)src+offset, chunk_size);
/*        for(i=0; i<10000; i++){
            gettimeofday(&tv, NULL); 
            f2 = tv.tv_usec;
            delta = f2-f1;
        //    f1 =  f2;
    }
        printf("delta is %d\n", delta);
  */     }

    return 0;
}
static inline int copy_strided2continous(void *src, void *dst, size_t count, size_t block_length, size_t stride) {

 int i;

#pragma omp parallel for num_threads(pre_thread->n_prefetch_threads)
    for(i = 0; i<count; i++) {
        memcpy((char*)dst+i*block_length, (char*)src+i*stride, block_length); 

    }

}

static inline int copy_continous2strided(void *src, void *dst, size_t count, size_t block_length, size_t stride) {

 int i;

#pragma omp parallel for num_threads(pre_thread->n_prefetch_threads)
    for(i = 0; i<count; i++) {
        memcpy((char*)dst+i*stride, (char*)src+i*block_length, block_length); 

    }

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


    int type;
    struct prefetch_task *task;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &type);
    int i;

    omp_set_dynamic(0);
    omp_set_num_threads(pre_thread->n_prefetch_threads);
#if 1
#pragma omp parallel
    {
        int id = omp_get_thread_num();
        if(pre_thread->init == 0 && pre_thread->cpusets) {
            hwloc_set_cpubind    (pre_thread->topology,pre_thread->cpusets[id], HWLOC_CPUBIND_THREAD);
    }

#pragma omp master
        pre_thread->init = 1;
    }
#endif

#ifdef SLEEP
       pthread_mutex_lock(&pre_thread->mv);
#endif
    while (1) {
#ifdef SLEEP
      volatile int wake = pre_thread->wakeup;
        while(wake!=1) {
            pthread_cond_wait(&pre_thread->cv, &pre_thread->mv);
            pthread_mutex_unlock(&pre_thread->mv);
            wake = pre_thread->wakeup;
        }
#endif

        volatile int done = pre_thread->task_list[pre_thread->current_out].done;
        while (!done) {
            task = &pre_thread->task_list[pre_thread->current_out++];
            if (pre_thread->current_out == pre_thread->n_tasks)
                pre_thread->current_out = 0;
            switch (task->task) {
            case (FETCH_CONTIG):
                 copy_continous(task->kind.continous.source, task->dest, task->kind.continous.size);
                break;
            case (FETCH_STRIDED):
                copy_strided2continous(task->kind.strided.source, task->dest, task->kind.strided.count, task->kind.strided.block_len, task->kind.strided.stride);
                break;
             case (WRITE_STRIDED):
                copy_continous2strided(task->kind.strided.source, task->dest, task->kind.strided.count, task->kind.strided.block_len, task->kind.strided.stride);
                break; 
            case (FETCH_LIST):
                copy_noncontinous(task->kind.list.source_start, task->dest,
                                  task->kind.list.elem_size, task->kind.list.indexes,
                                  task->kind.list.elements, &task->index);
                break;
            default:
                printf("Failure in copy thtrad\n");
            };
            lock(&pre_thread->mp);

 	    _mm_sfence();
            pre_thread->open_tasks--;
            task->done = 1;
            unlock(&pre_thread->mp);

            done = pre_thread->task_list[pre_thread->current_out].done;

        }
    }
}

#ifdef USE_EMU
inline prefetch_handle_t start_prefetch_continous(void *src, void *dst, size_t offset, size_t size) {
#else
inline    prefetch_handle_t start_prefetch_continous(void *src, void *dst, size_t size) {
#endif

        volatile int free;
        struct prefetch_task *task;

       lock(&pre_thread->mp);
       task = &(pre_thread->task_list[pre_thread->current_in++]);
        if(pre_thread->current_in == pre_thread->n_tasks)
            pre_thread->current_in= 0;
        free = task->done;

        if (free == 0 )  {
            unlock(&pre_thread->mp);
            while (free == 0){
                free = task->done;
            }

            lock(&pre_thread->mp);
        }
        task->dest  = dst;
        task->kind.continous.source = src;
        task->kind.continous.size = size;
#ifdef USE_EMU
        task->offset = offset;
#endif
        task->task = FETCH_CONTIG;
        pre_thread->open_tasks++;
 	 _mm_sfence();

        task->done = 0;
        unlock(&pre_thread->mp);
        return task->handle;

 }

inline    prefetch_handle_t start_prefetch_strided(void *src, void *dst, size_t count, size_t block_length, size_t stride) {

    volatile int free;
    struct prefetch_task *task;
    lock(&pre_thread->mp);
    task = &(pre_thread->task_list[pre_thread->current_in++]);
    if(pre_thread->current_in == pre_thread->n_tasks)
        pre_thread->current_in= 0;
    free = task->done;

    if (free == 0 )  {
        unlock(&pre_thread->mp);
        while (free == 0){
            free = task->done;
        }

        lock(&pre_thread->mp);
    }
    task->dest  = dst;
    task->kind.strided.source = src;
    task->kind.strided.block_len = block_length;
    task->kind.strided.stride = stride;
    task->task = FETCH_STRIDED;
    pre_thread->open_tasks++;
    _mm_sfence();

    task->done = 0;
    unlock(&pre_thread->mp);
    return task->handle;

}
inline    prefetch_handle_t start_write_back_strided(void *src, void *dst, size_t count, size_t block_length, size_t stride) {

    volatile int free;
    struct prefetch_task *task;
    lock(&pre_thread->mp);
    task = &(pre_thread->task_list[pre_thread->current_in++]);
    if(pre_thread->current_in == pre_thread->n_tasks)
        pre_thread->current_in= 0;
    free = task->done;

    if (free == 0 )  {
        unlock(&pre_thread->mp);
        while (free == 0){
            free = task->done;
        }

        lock(&pre_thread->mp);
    }
    task->dest  = dst;
    task->kind.strided.source = src;
    task->kind.strided.block_len = block_length;
    task->kind.strided.stride = stride;
    task->task = WRITE_STRIDED;
    pre_thread->open_tasks++;
    _mm_sfence();

    task->done = 0;
    unlock(&pre_thread->mp);
    return task->handle;

}

inline    prefetch_handle_t start_writeback_strided(void *src, void *dst, size_t count, size_t block_length, size_t stride) {

    volatile int free;
    struct prefetch_task *task;

    lock(&pre_thread->mp);
    task = &(pre_thread->task_list[pre_thread->current_in++]);
    if(pre_thread->current_in == pre_thread->n_tasks)
        pre_thread->current_in= 0;
    free = task->done;

    if (free == 0 )  {
        unlock(&pre_thread->mp);
        while (free == 0){
            free = task->done;
        }

        lock(&pre_thread->mp);
    }
    task->dest  = dst;
    task->kind.strided.source = src;
    task->kind.strided.block_len = block_length;
    task->kind.strided.stride = stride;
    task->task = WRITE_STRIDED;
    pre_thread->open_tasks++;
    _mm_sfence();

    task->done = 0;
    unlock(&pre_thread->mp);
    return task->handle;

}
#ifdef USE_EMU
inline prefetch_handle_t start_prefetch_noncontinous(void *src_start, void *dst, size_t offset, int* offsets, size_t element_size, int elements) {
#else
inline prefetch_handle_t start_prefetch_noncontinous(void *src_start, void *dst, int* offsets, size_t element_size, int elements) {
#endif

    int free;
    lock(&pre_thread->mp);
    struct prefetch_task *task = &(pre_thread->task_list[pre_thread->current_in++]);
    free = task->done;
    if ((!free))  {
        unlock(&pre_thread->mp);
        while (free){
            free = task->done;
        }

        lock(&pre_thread->mp);
    }

    if(pre_thread->current_in == pre_thread->n_tasks)
       pre_thread->current_in= 0;


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
    pre_thread->open_tasks++;
    task->done = 0;
    unlock(&pre_thread->mp);

    return task->handle;


}

inline int prefetch_wakeup(){

   pthread_mutex_lock(&pre_thread->mv);
    pre_thread->wakeup = 1;
    pthread_mutex_unlock(&pre_thread->mv);
   pthread_cond_signal(&pre_thread->cv);

    return 0;
}

inline int prefetch_allow_sleep() {
  pthread_mutex_lock(&pre_thread->mv);
   pre_thread->wakeup = 0;
   pthread_mutex_unlock(&pre_thread->mv);

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

    struct prefetch_task  *tasks;
    int i;
    pre_thread = (struct prefetch_thread *)malloc(sizeof(struct prefetch_thread));
    assert(pre_thread);
    tasks = (struct prefetch_task*)malloc(sizeof(struct prefetch_task)* queue_length);
    assert(tasks);
    for (i = 0; i< queue_length; i++) {
        tasks[i].done = 1;
        tasks[i].handle = (uint64_t)&tasks[i];
    }
    pre_thread->cpusets = NULL;
    pre_thread->task_list = tasks;
    pre_thread->current_in = 0;
    pre_thread->current_out = 0;
    pre_thread->open_tasks = 0;
    pre_thread->n_tasks = queue_length;
    pthread_cond_init(&pre_thread->cv, NULL);
    pthread_mutex_init(&pre_thread->mv, NULL);
    pre_thread->mp = 0;
    pre_thread->wakeup = 0;
    pre_thread->init = 0;
    pre_thread->n_prefetch_threads = prefetch_threads;

    pthread_create(&pre_thread->thread, NULL,
                         exec_prefetch_thread, NULL);
    //pthread_detach(pre_thread->thread);
    return pre_thread->handle;
}
inline prefetch_thread_t create_new_thread_with_topo(int queue_length, hwloc_topology_t topology, hwloc_cpuset_t  *cpusets, int n_threads) {

    struct prefetch_task  *tasks;
    int i;
    pre_thread = (struct prefetch_thread *)malloc(sizeof(struct prefetch_thread));
    assert(pre_thread);
    tasks = (struct prefetch_task*)malloc(sizeof(struct prefetch_task)* queue_length);
    assert(tasks);

    for (i = 0; i< queue_length; i++) {
        tasks[i].done = 1;
        tasks[i].handle = (uint64_t)&tasks[i];
    }
    pre_thread->topology = topology;

    pre_thread->cpusets = cpusets;

    pre_thread->task_list = tasks;
    pre_thread->current_in = 0;
    pre_thread->current_out = 0;
    pre_thread->open_tasks = 0;
    pre_thread->n_tasks = queue_length;
    pthread_cond_init(&pre_thread->cv, NULL);
    pthread_mutex_init(&pre_thread->mv, NULL);
    pre_thread->mp = 0;
    pre_thread->wakeup = 0;
    pre_thread->init = 0;


    pre_thread->n_prefetch_threads = n_threads;

    pthread_create(&pre_thread->thread, NULL,
                         exec_prefetch_thread, NULL);
 //   pthread_detach(pre_thread->thread);
//    sleep(4);
    return 0;
}


inline void finish_thread() {

    int out;
    
    while(out) {
        out  = pre_thread->open_tasks;
    };
    printf("out is %d\n", count);
    if(pre_thread->thread)
        pthread_cancel(pre_thread->thread);
    free(pre_thread->task_list);
    free(pre_thread);
}

#endif
