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
#define ITER 20
#endif
#ifdef USE_PAPI
int EventSet[3] ={ PAPI_NULL, PAPI_NULL, PAPI_NULL};

#pragma omp threadprivate(EventSet)
#endif
int id;
#pragma omp threadprivate(id)



//#define max_size (4*1024*1024)

void init(int* field, size_t s) {
	int i,k;

#pragma omp parallel for 
		for(i = 0; i< s; i++) {
			field[i] = i/1024;

		}

}

int main (int argc, char *argv[])
{

   int *fieldA, *fieldB;
    int i,t;

    struct aclock timer;
    int threads = 1;
    hwloc_cpuset_t *cpusets;
    hwloc_topology_t topology;
    hwloc_obj_t *roots;
    unsigned int nroots;
    size_t max_size = 1024; 
    init_aclock(&timer);
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
  ret = PAPI_library_init(PAPI_VER_CURRENT);
    if (ret != PAPI_VER_CURRENT) {
        printf("PAPI library init error!\n");
        return -1;
    }
    if (PAPI_thread_init(pthread_self) != PAPI_OK)
        abort();


#endif

    if(argc >= 2){
        threads = atoi(argv[1]);
    }
   if(argc >=3){
        max_size = atoi(argv[2]);
 	}
     max_size*=(1024*1024); 
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);
    nroots = hwloc_get_nbobjs_by_depth(topology, 0);
    roots = (hwloc_obj_t *) malloc(nroots*sizeof(hwloc_obj_t));
    for(i = 0; i < nroots; i++)
       roots[i] = hwloc_get_obj_by_depth(topology, 0, i);
     cpusets = (hwloc_cpuset_t *) malloc((threads)*sizeof(hwloc_bitmap_t));

    hwloc_distrib(topology, roots, nroots,cpusets, threads, INT_MAX, 0); 


     omp_set_num_threads(threads);
omp_set_dynamic(0);
#pragma omp parallel
     {
         int id = omp_get_thread_num();
         hwloc_set_cpubind    (topology, cpusets[id], HWLOC_CPUBIND_THREAD);
}

    srand(time(NULL));

#ifdef HBW_MEM

    if((double) max_size >(1024.0*1024.0*1024.0*4))
        return 0;
    fieldA = (int*)memkind_malloc(MEMKIND_HBW,sizeof(char)* (max_size));

#else

    fieldA = (int*)malloc(sizeof(char)*(max_size));
#endif	
     size_t copy_size = 1024;
     size_t chunk = 1;

    init(fieldA, max_size/sizeof(int));
    int k;
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


	#define	DOIT(i)	p[i] = 1;
    for(copy_size = max_size; copy_size<=max_size; copy_size*=2)
{
    memset(fieldA, 0x0, copy_size);
    chunk = copy_size/sizeof(int)/4;
    start_aclock(&timer);
#ifdef USE_PAPI
#pragma omp parallel

    if(id <64)
     if(PAPI_start (EventSet[ev])){
        printf("error starting countee\n");
        abort();
    }
#endif


    if(copy_size > 1024*1024*1024) {
        for(t = 0; t<ITER; t++){
            for(k = 0; k<4; k++){
#pragma omp parallel for  firstprivate(k)
                for (i=0; i<chunk; i++) {
                    fieldA[i+chunk*k] = t;
                }

            }

        }
    }
    else {
        for(t = 0; t<ITER; t++){
#pragma omp parallel for
            for (i=0; i<max_size/sizeof(int); i++) {
                fieldA[i] = t;
            }

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




    printf("Timer %d %d \t %f\n", threads,max_size/(1024*1024),  (double)get_seconds(&timer)/ITER);
}
}

