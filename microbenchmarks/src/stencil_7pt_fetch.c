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

#define ind(z,y,x) (z*(size_y+2)*(size_x+2)+(y)*(size_x+2)+x)

#define min(a,b)  ((a)<(b) ? (a):(b))
#define max(a,b)  ((a)>=(b) ? (a):(b))

int CHUNK = 32;

static inline int determine_chunk(int x, int y, int threads) {

    size_t sizexy = (x+2) *(y+2);
   size_t  avail = hexe_avail_mcdram(); 
   int chunk_per_thread =  avail/(threads * sizexy * sizeof(double) * 8 );
    return chunk_per_thread * threads; 

}
int id, threadchunk;
#pragma omp threadprivate(id, threadchunk)


double fRand(double fMin, double fMax)
{
    double f = (double)434.43234 / RAND_MAX;
    return fMin + f * (fMax - fMin);
}
void init(double* in, int x, int y, int z) {
    int i,j,l;
#pragma omp parallel for
    for(i = 0; i< z; i++)
        for(j = 1; j < y; j++)
            for(l= 1; l<x; l++)
            in[i*x*y+j*y+l] = fRand(0.0, 5.0);
}

void init_0(double* in, int x, int y) {
    int i,j;
    for(i = 0; i< y; i++)
        for(j = 1; j < x; j++)
            in[i*x+y] = 0.0;
}

int main (int argc, char *argv[])
{
    size_t size_x, size_y, size_z,  sizexyz;
//    size_t prefetch_size, prefetch_offset;
    double* __restrict__ fieldA_all;
    double* __restrict__ fieldB_all;



    int row;
    int iter;
    int threads;
    int ret;

   int need_fetch;
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
    size_x = 64;
    size_y = 64;
    size_z = 64;
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
       size_z = size_x;
    }
    if(argc >= 3){
        size_x = atoi(argv[1]);
        size_y = atoi(argv[2]);

     if(argc >= 4 )
	size_z = atoi(argv[3]);
        if(argc >= 5)
            threads = atoi(argv[4]);
        if(argc >= 6)
            iter = atoi(argv[5]);

    }
    sizexyz = (size_y+2)*(size_x+2)*(size_z+2);
    printf("Calculate a stencil for a matrix of size %dX%d for %d iterations\n",
            size_x, size_y,iter);
 

    fieldA_all = hexe_request_hbw(NULL, sizeof(double)*sizexyz, 7);
    fieldB_all = hexe_request_hbw(NULL, sizeof(double)*sizexyz, 5);

     hexe_bind_requested_memory(0);
	printf("A %p B %p \n", fieldA_all, fieldB_all);

   need_fetch = !(hexe_is_in_hbw (fieldA_all) && hexe_is_in_hbw(fieldB_all));

   if(need_fetch) {
        CHUNK = determine_chunk(size_x, size_y, threads);
        hexe_set_compute_threads(threads);
     	hexe_set_prefetch_threads(16); 
        hexe_alloc_pool ((size_x+2)*(size_y+2)*(CHUNK+2)*sizeof(double), 2);


    printf("here pool size is %ld\n", (size_x+2)*(size_y+2)*(CHUNK+2)*sizeof(double)/(1024*1024) );

    }
    else{
        hexe_set_prefetch_threads(0); 
        hexe_set_compute_threads(threads); 
        CHUNK=size_z;
}

      printf("I would get a chunk of %ld \n", CHUNK);
       hexe_start();
    printf("start?\n");
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

 init(fieldA_all, size_x, size_y, size_z);
    init(fieldB_all, size_x, size_y, size_z);
printf("Init done\n");
 
 start_aclock(&timer);

#pragma omp parallel
{

    int h,i, j, t, k;
    int  need_fetch;
    int q = 0;
    size_t prefetch_size, prefetch_offset;
    double* __restrict__ fieldA = fieldA_all;
    double* __restrict__ fieldB = fieldB_all;
    double* tmp;

    int loops = size_z/CHUNK;
    if (loops % size_z != 0)
        loops+1;
    double*  cache;
    id =omp_get_thread_num(); 
    threadchunk = CHUNK/threads;

    for(t = 0; t<iter; t++){
        need_fetch = !(hexe_is_in_hbw (fieldA)); 

#ifdef USE_PAPI

        if(t == 2)
            PAPI_start (EventSet);//= PAPI_OK)
#endif


#pragma omp master
        if(need_fetch) {
            prefetch_size = (CHUNK+2)*(size_x+2)*(size_y+2)*sizeof(double);
            hexe_start_fetch_continous_taged(fieldA, prefetch_size, 0,0);
        }

        for (k = 0; k<loops; k++) {
         threadchunk =  min(CHUNK, (size_z-(CHUNK*k)))/threads;

            int start = 1 +CHUNK*k+threadchunk*id;
            int end = min((start+threadchunk), (size_z+1));
            // printf("id: %d start %d, end %d\n",id, start, end);
           if(need_fetch) {
                size_t prefetch_offset = (start+CHUNK-1)*(size_x+2)*(size_y+2);
                size_t prefetch_size = (size_x+2)* (size_y+2) * sizeof(double) * min((CHUNK+2), max(0, (long long)(size_z-(k+1)*CHUNK+2)));
                //            printf("start %d, size %d \n", start,  min((CHUNK+2), (size_z-(k+1)*CHUNK+1))); 

#pragma omp master
                {
                    if(prefetch_size>2) 
                        hexe_start_fetch_continous_taged(fieldA+prefetch_offset, prefetch_size, 1-q, k+1);
                }
                cache = hexe_sync_fetch_taged(q,k)+threadchunk*id*(size_x+2)*(size_y+2);
                q=1-q;
            }
            else {
                cache = &fieldA[ind((start-1),0,0)];
            }

            for(h = start; h<end; h++){
                int l = h - start+1;
                for(i = 1; i<size_y+1; i++) {
                    for(j = 1; j <size_x+1; j++) {
                        fieldB[ind(h,i,j)] =
                            0.5*cache[ind(l,i,j)]+0.5*
                            (0.1*cache[ind(l,(i+1),j)]+0.1*cache[ind(l,(i-1),j)]+
                             0.1*cache[ind(l,i,(j+1))]+0.1*cache[ind(l,i,(j-1))]+
                             0.1*cache[ind((l+1),i, j)]+ 0.1*cache[ind((l-1),i,j)] )  ;

                    }
                }
            }
        }
#pragma omp barrier
        tmp = fieldB;
        fieldB = fieldA;
        fieldA = tmp;

    }

#ifdef USE_PAP
    long long value_p[3];
    PAPI_stop (EventSet, value_p);// != PAPI_OK)

#pragma omp critical
    {
        int n;
        for(n=0; n<2; ++n) {
            value[n] += value_p[n];
        }
    }

#endif
}


    end_aclock(&timer);


    hexe_free_memory(fieldA_all);
    hexe_free_memory(fieldB_all);
 
 //       return ret;
    printf("res: %d\t %d\t %d\t %4.2f \n",size_x, size_y, iter,  get_seconds(&timer));
  //  hexe_finalize();
    printf("ende?\n");

#ifdef USE_PAPI
    PAPI_shutdown();
#endif
}

