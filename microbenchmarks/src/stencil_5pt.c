
#include "timer.h"
#include <hexe.h>
#ifdef USE_PAPI
#include <papi.h>
#endif

#define ind(y,x) (y)*(size_x+2)+x

double fRand(double fMin, double fMax)
{
    double f = (double)23423.44 / RAND_MAX;
    return fMin + f * (fMax - fMin);
}

void init(double* in, size_t x, size_t y) {
    int i,j;
#pragma omp parallel for
    for(i = 0; i< y; i++)
        for(j = 0; j < x; j++)
            in[i*x+j] = fRand(0.0, 5.0);
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

    int i, j, t;
    int row;
    int iter;
    int threads;
    int ret;

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

   hexe_set_prefetch_threads(0);
    hexe_set_compute_threads(threads);
    hexe_start();


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

    fieldA = hexe_request_hbw(NULL, sizeof(double)*sizexy, 7);
    fieldB = hexe_request_hbw(NULL, sizeof(double)*sizexy, 5);

     hexe_bind_requested_memory(0);
      init(fieldA, size_x, size_y);
     init(fieldB, size_x, size_y);
    printf("init done\n");

 start_aclock(&timer);
 for(t = 0; t<iter; t++){

#ifdef USE_PAPI
#pragma omp parallel
  if(t == 2)
 PAPI_start (EventSet);//= PAPI_OK)
#endif


#pragma omp parallel for private(j)
     for(i = 1; i<size_y+1; i++) {
         for(j = 1; j <size_x+1; j++) {
                fieldB[ind(i,j)] =
                    0.5*fieldA[ind(i,j)]+0.5*
                    (0.1*fieldA[ind((i+1),j)]+0.1*fieldA[ind((i-1),j)]+
                     0.1*fieldA[ind(i,(j+1))]+0.1*fieldA[ind(i,(j-1))]);

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


#ifdef USE_PAPI
    PAPI_shutdown();
#endif
}

