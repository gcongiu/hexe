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
#define SIZE (1024*1024*488)
#else
#define ITER 16
#define SIZE (1024*1024*488)
#endif
#ifdef USE_PAPI
int EventSet[3] ={ PAPI_NULL, PAPI_NULL, PAPI_NULL};

#pragma omp threadprivate(EventSet)
#endif
int id;
#pragma omp threadprivate(id)



void init(double* field) {
	int i,k;

	for(k = 0; k< 2; k ++) {
#pragma omp parallel for 
		for(i = 0; i< SIZE; i++) {
			field[i+k*SIZE] = (double)i/1024.0;

		}
	} 
}
int main (int argc, char *argv[])
{

	double*  fieldA, *fieldB;
	size_t i,k;
	double goal;
	int t;
	struct aclock timer;
	int threads = 1;
    hwloc_cpuset_t *cpusets;
    hwloc_topology_t topology;
    hwloc_obj_t *roots;
    unsigned int nroots;


	if(argc >= 2){
		threads = atoi(argv[1]);
	}
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
         id = omp_get_thread_num();
         hwloc_set_cpubind    (topology, cpusets[id], HWLOC_CPUBIND_THREAD);
}

#ifdef USE_PAPI
   int ev, ret;
    PAPI_event_info_t evinfo[3];
    long long value[3] = {0,0,0};
/*    const int eventlist[3] = {
    PAPI_L2_TCM,
    PAPI_L2_LDM,
    PAPI_L2_TCH
};

 char* cevents[3] = {
    "PAPI_L2_TCM",
    "PAPI_L2_LDM",
    "PAPI_L2_TCH"
   };

*/
	int eventlist[3];

    char* cevents[3] = {
    "LLC_RQSTS",
    "LLC-PREFETCHES",
    "LLC-PREFETCH-MISSES"
    };
  
  ret = PAPI_library_init(PAPI_VER_CURRENT);
    if (ret != PAPI_VER_CURRENT) {
        printf("PAPI library init error!\n");
        return -1;
    }
    if (PAPI_thread_init(pthread_self) != PAPI_OK)
        abort();

#endif



	init_aclock(&timer);
#ifdef HBW_MEM
	fieldA = memkind_malloc(MEMKIND_HBW,2*sizeof(double)*(SIZE+3*1024*1024));
	fieldB = memkind_malloc(MEMKIND_DEFAULT, 2*sizeof(double)*SIZE);

#else
	fieldA = malloc( 2*sizeof(double)*(SIZE+3*1024*1024));
	fieldB = malloc( 2*sizeof(double)*SIZE);
#endif


	init(fieldA); 
	init(fieldB);

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
/*        if (PAPI_query_event (eventlist[ev]) != PAPI_OK){
             printf("Could not query event %s 0\n", evinfo[ev].symbol);
         }
*/
#pragma omp master
  if (ret =PAPI_event_name_to_code(cevents[ev], &eventlist[ev]) != PAPI_OK){
         if(ret != PAPI_OK) {
                 printf("Could not translate event %s %d \n %d %s\n",  cevents[ev], omp_get_thread_num(), ret, PAPI_strerror(ret));
                 abort();
   }
    }

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
		for(k = 0; k< 2; k ++) {
#pragma omp parallel for firstprivate(k)
			for(i = 0; i< SIZE; i++) {
			fieldB[k*SIZE+i]  = fieldA[k*SIZE+i]+
				      fieldA[k*SIZE+i+1024*1024];
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
#pragma omp master
    printf("\t \t %s\t\t%s\t\t%s\n", cevents[0], cevents[1],cevents[2]);
    printf("PAPI:\t %u\t\t%u\t\t%u\n", value[0], value[1],value[2]);
#endif


	printf("Timer %d \t %f\n", threads,  get_seconds(&timer)/ITER);

}
