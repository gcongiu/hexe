#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "timer.h"
#include <omp.h>
#include <xmmintrin.h>
#include <sys/mman.h>
#include <hexe.h>
#ifdef USE_PAPI
#include <papi.h>
#endif
#include<pthread.h>
#define ind(y,x) (y)*(size_x+2)+x
#define max(a,b) ((a)>=(b) ? (a):(b))
#define CHUNK 1024
#define min(a,b)  ((a)<(b) ? (a):(b))
int id, threadchunk;
#pragma omp threadprivate(id, threadchunk)



double fRand(double fMin, double fMax)
{
    double f = (double)rand() / RAND_MAX;
    return fMin + f * (fMax - fMin);
}

void init(double* in, int x, int y) {
    int i,j;
    for(i = 0; i< y; i++)
        for(j = 1; j < x; j++)
            in[i*x+y] = fRand(0.0, 5.0);
}
void init_0(double* in, int x, int y) {
    int i,j;
    for(i = 0; i< y; i++)
        for(j = 1; j < x; j++)
            in[i*x+y] = 0.0;
}

int main (int argc, char *argv[])
{
    size_t size_x, size_y, sizexy;

    double* __restrict__ fieldA;
    double* __restrict__ fieldB;
    double* tmp;

    int i, j, t, k;
    int row;
    int iter;
    int threads;
    int ret;
    int need_fetch;
    size_t prefetch_size;

#ifdef USE_PAPI
    PAPI_event_info_t evinfo[3];
    long long value[3] = {0,0,0};
    const int eventlist[3] = {
        PAPI_L1_TCM,
        PAPI_L2_TCM,
        PAPI_L3_TCM
    };

#endif
    struct aclock timer;

#ifdef USE_PAPI
    for (i = 0; i<3; i++)
        PAPI_get_event_info( eventlist[i], &evinfo[i] );

#endif
    hexe_init();
    size_x = 1024;
    size_y = 1024;
    iter = 100;
    threads = 1;
    srand(time(NULL));

    init_aclock(&timer);
    /* Init PAPI*/
#ifdef USE_PAPI
    ret = PAPI_library_init(PAPI_VER_CURRENT);
    if (ret != PAPI_VER_CURRENT) {
        printf("PAPI library init error!\n");
        return -1;
    }
    if (PAPI_thread_init(pthread_self) != PAPI_OK)
        exit(1);
#endif

    if(argc == 2){
        size_x = atoi(argv[1]);
        size_y = size_x;
    }
    if(argc > 2){
        size_x = atoi(argv[1]);
        size_y = atoi(argv[2]);
        if(argc >= 4)
            threads = atoi(argv[3]);
        if(argc >= 5)
            iter = atoi(argv[4]);

    }
    sizexy = (size_y+2)*(size_x+2);
    printf("Calculate a stencil for a matrix of size %dX%d for %d iterations\n",
            size_x, size_y,iter);

    omp_set_num_threads(threads);

#ifdef USE_PAPI

    omp_set_dynamic(0);
#pragma omp parallel
    {


        /* Here we create the eventset */
        if ((ret=PAPI_create_eventset (&EventSet)) != PAPI_OK)
            printf("Could not create private event set \n");

        if (PAPI_query_event (eventlist[0]) != PAPI_OK){
            printf("Could not query event %s\n", evinfo[0].symbol);
        }

        if (PAPI_add_event (EventSet, eventlist[0]) != PAPI_OK){
            printf("Could not add event %s",  evinfo[0].symbol);
        }

        if (PAPI_query_event (eventlist[1]) != PAPI_OK){
            printf("Could not query event %s\n",  evinfo[1].symbol);
        }

        if (PAPI_add_event (EventSet, eventlist[1]) != PAPI_OK){
            printf("Could not add event %s\n",  evinfo[1].symbol);
        }

        if (PAPI_query_event (eventlist[2]) != PAPI_OK){
            printf("Could not query event %s\n",  evinfo[2].symbol);
        }


        if (PAPI_add_event (EventSet, eventlist[2]) != PAPI_OK){
            printf("Could not add event %s\n",  evinfo[2].symbol);
            // return -1;
        }
    }
#endif

    fieldA = hexe_request_hbw(NULL, sizeof(double)*sizexy, 5);
    fieldB = hexe_request_hbw(NULL, sizeof(double)*sizexy, 5);

    hexe_bind_requested_memory(0);
    need_fetch = !(hexe_is_in_hbw (fieldA) && hexe_is_in_hbw(fieldB));
 
   if(need_fetch) {
        hexe_set_compute_threads(threads);
     	  hexe_set_prefetch_threads(32); 
        hexe_alloc_pool ((size_x+2)*(CHUNK+2)*sizeof(double), 2);
        hexe_start();

printf("here pool size is %ld\n", (size_x+2)*(CHUNK+2)*sizeof(double)/(1024*1024) );

    }

    start_aclock(&timer);
    for(t = 0; t<iter; t++){
        double  *__restrict__ cache;
         need_fetch = !(hexe_is_in_hbw (fieldA));

#ifdef USE_PAPI
#pragma omp parallel
        if(t == 2)
            PAPI_start (EventSet);//= PAPI_OK)
#endif 
        int q=0;
        if(need_fetch) {
            prefetch_size = (CHUNK+2)*(size_x+2)*sizeof(double);
            hexe_start_fetch_continous(fieldA, prefetch_size, 0);
        }
 int loops = size_y/CHUNK;
    // printf("lop is %d\n", loops);

#pragma omp parallel private(k) firstprivate(q)
{
    id =omp_get_thread_num(); 
    threadchunk = CHUNK/threads;
        for (k = 0; k<loops; k+= 1) {
        int start = 1 +CHUNK*k+threadchunk*id;
        int end = min((start+threadchunk), (size_y));
//              printf("from %d to %d\n", start, end);
           if(need_fetch) {
                prefetch_size = (size_x+2) * sizeof(double) * min(CHUNK, (size_y-(k+1)*CHUNK+1));
                size_t prefetch_offset = (k+CHUNK-1)*(size_x+2);
               if(prefetch_size>0) 
                    hexe_start_fetch_continous(fieldA+prefetch_offset, prefetch_size, 1-q);
                cache = hexe_sync_fetch(q);
                q=1-q;
            }
            else {
                cache = &fieldA[ind(start-1,0)];
            }
            for(i = start; i<end; i++) {
                int l = i - k+1;
                for(j = 1; j <size_x+1; j++) {
                    fieldB[ind(i,j)] =
                        0.5*cache[ind(l,j)]+0.5*
                        (0.1*cache[ind((l+1),j)]+0.1*cache[ind((l-1),j)]+
                         0.1*cache[ind(l,(j+1))]+0.1*cache[ind(l,(j-1))]);

                }
            }
        }
}
#ifndef COPY_DATA
        tmp = fieldB;
        fieldB = fieldA;
        fieldA = tmp;
#else
#pragma omp parallel 
        {
            int id = omp_get_thread_num();
            int chunk_size = sizexy/threads;
            int offset = chunk_size *id;
            if(id == threads-1){
                chunk_size +=  sizexy%threads;
            }
            memcpy((char*)(fieldA+offset),(char*) (fieldB+offset), chunk_size*sizeof(double));
        }

#endif

#ifdef MOVE_PAGES
        hexe_change_priority(fieldA,  7);
        hexe_change_priority(fieldB,  5);
        hexe_bind_requested_memory(1);
#endif


    }

#ifdef USE_PAPI
#pragma omp parallel
    {
        long long value_p[3];
        PAPI_stop (EventSet, value_p);// != PAPI_OK)

        end_aclock(&timer);
        printf(" size \t %s\t %s\t %s\n",evinfo[0].short_descr ,evinfo[1].short_descr,evinfo[2].short_descr);
        printf("PAPI: %d\t %u\t\t%u\t\t%u\n", size_x, value_p[0], value_p[1],value_p[2]);

#pragma omp critical
        {
            int n;
            for(n=0; n<2; ++n) {
                value[n] += value_p[n];
            }
        }
    }
#else
    end_aclock(&timer);

#endif
    //       return ret;
    printf("res: %d\t %d\t %d\t %4.2f \n",size_x, size_y, iter,  get_seconds(&timer));
    printf("here 1\n");
  if(need_fetch)
        hexe_finalize();

    printf("here 2\n");
    hexe_free_memory(fieldA);
    hexe_free_memory(fieldB);
#ifdef USE_PAPI
    PAPI_shutdown();
#endif
    printf("here?\n");
}

