#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "timer.h"
#include <omp.h>
#include <memkind.h>
#include <sys/mman.h>
#include<pthread.h>
#include <hwloc.h>
#ifdef USE_PAPI
#include "papi.h"
#define ITER 2
#else
#define ITER 100
#endif
#define SIZE (1024*1024)
#define POOL_SIZE (16*1024*1024)

#ifdef USE_PAPI
int EventSet[3] ={ PAPI_NULL, PAPI_NULL, PAPI_NULL};

#pragma omp threadprivate(EventSet)
#endif
int id;
#pragma omp threadprivate(id)

void init(int* field, int size) {
	int i,k;
        for(i = 0; i< size; i++) {
            field[i] = i/1024;

        }
 }
int main (int argc, char *argv[])
{

    int*  fieldA;
    size_t i,k;
    double goal;
    int t;
    struct aclock timer;
    int threads = 1;
    int prefetch_threads = 0;
    hwloc_cpuset_t *cpusets;
    hwloc_topology_t topology;
    hwloc_obj_t *roots;
    unsigned int nroots;
    int stride = 1;

    if(argc >= 2){
        threads = atoi(argv[1]);
    } 
   if(argc >= 3){
        stride = atoi(argv[2]);
    }
   hwloc_topology_init(&topology);
   hwloc_topology_load(topology);
    nroots = hwloc_get_nbobjs_by_depth(topology, 0);
    roots = (hwloc_obj_t *) malloc(nroots*sizeof(hwloc_obj_t));
    for(i = 0; i < nroots; i++)
       roots[i] = hwloc_get_obj_by_depth(topology, 0, i);
     cpusets = (hwloc_cpuset_t *) malloc((prefetch_threads+threads)*sizeof(hwloc_bitmap_t));

    hwloc_distrib(topology, roots, nroots,cpusets, threads+prefetch_threads, INT_MAX, 0); 

#ifdef USE_PAPI
   int ev, ret;
    PAPI_event_info_t evinfo[3];
    long long value[3] = {0,0,0};
    const int eventlist[3] = {
    PAPI_TLB_DM,
    PAPI_L1_TCM,
    PAPI_L2_TCA
};

 char* cevents[3] = {
    "PAPI_TLB_DM",
    "PAPI_L1_TCM",
    "PAPI_L2_TCA"
   };

/*
	int eventlist[3];

    char* cevents[3] = {
    "LLC_RQSTS",
    "LLC-PREFETCHES",
    "LLC-PREFETCH-MISSES"
    };
  */
  ret = PAPI_library_init(PAPI_VER_CURRENT);
    if (ret != PAPI_VER_CURRENT) {
        printf("PAPI library init error!\n");
        return -1;
    }
    if (PAPI_thread_init(pthread_self) != PAPI_OK)
        abort();

#endif



     omp_set_num_threads(threads);
 omp_set_dynamic(0);
#pragma omp parallel
     {
         int id = omp_get_thread_num();
         hwloc_set_cpubind    (topology, cpusets[id], HWLOC_CPUBIND_THREAD);
}

#ifdef HBW_MEM
if( stride*sizeof(int)*(SIZE)>=(8192*1024*1024-1))
        return 0;
    fieldA = memkind_malloc(MEMKIND_HBW_HUGETLB, stride*sizeof(int)*(SIZE));
    if(fieldA == NULL) {
    printf("could not allocat Huge Pages \n");
    fieldA = memkind_malloc(MEMKIND_HBW, stride*sizeof(int)*(SIZE));
}
#else
    fieldA = malloc(stride*sizeof(int)*(SIZE));
#endif

    init(fieldA, SIZE*stride); 

#ifdef USE_PAPI
 for(ev = 0; ev<3; ev++){
#pragma omp parallel
     {
         int id = omp_get_thread_num();
         /* Here we create the eventset */
         if ((ret=PAPI_create_eventset (&EventSet[ev])) != PAPI_OK) {
             printf("Could not create private event set %d \n", ev);
           abort();
         }
        if (PAPI_query_event (eventlist[ev]) != PAPI_OK){
             printf("Could not query event %s 0\n", evinfo[ev].symbol);
         }
/*
#pragma omp master
  if (ret =PAPI_event_name_to_code(cevents[ev], &eventlist[ev]) != PAPI_OK){
         if(ret != PAPI_OK) {
                 printf("Could not translate event %s %d \n %d %s\n",  cevents[ev], omp_get_thread_num(), ret, PAPI_strerror(ret));
                 abort();
   }
    }
*/
#pragma omp barrier
    if(id <64)
      if (ret =PAPI_add_event( EventSet[ev], eventlist[ev]) != PAPI_OK){
             if(ret != PAPI_OK) {
                 printf("Could not add event %s %d \n %d %s\n",  cevents[ev], omp_get_thread_num(), ret, PAPI_strerror(ret));
                 abort();
             }
         }
     }
#endif


    start_aclock(&timer);
#ifdef USE_PAPI
#pragma omp parallel

    if(id <64)
     if(PAPI_start (EventSet[ev])){
        printf("error starting countee\n");
        abort();
    }
#endif

   for(t = 0; t<ITER; t++) {
#pragma omp parallel for reduction(+ :goal) firstprivate(k)
            for(i = 0; i< SIZE; i++) {
                fieldA[i*stride] = i-1;
            }
        }
#ifdef USE_PAPI
#pragma omp parallel
{

    long long value_p = 0;

    if(id <64)
    if( PAPI_stop (EventSet[ev], &value_p) != PAPI_OK){
        printf("error stop counter");
        abort();
       }
     if(PAPI_reset(EventSet[ev]) != PAPI_OK) {
        printf("could not reset EventSet \n");
        abort;
    }
    end_aclock(&timer);
#pragma omp critical
    {
            value[ev] += value_p;
    }

//    printf("PAPI: %d\t %u\n", size_x, value_p[ev]);
}

}
#else
end_aclock(&timer);
#endif
#ifdef USE_PAPI
    printf("\t \t %s\t\t%s\t\t%s\n", cevents[0], cevents[1],cevents[2]);
    printf("PAPI:\t %u\t\t%u\t\t%u\n", value[0], value[1],value[2]);
#endif



printf("Timer %d %d \t %f\n", threads, stride*sizeof(int), get_seconds(&timer)/ITER);
ende:

#ifdef HBW_MEM
   memkind_free(MEMKIND_DEFAULT, fieldA);
#else
   free(fieldA);
#endif

    return 0;
}
