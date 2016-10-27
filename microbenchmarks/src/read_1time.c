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
#define ITER 32
#define SIZE (1024*1024*1024)

void init(char* field) {
	int i;
#pragma omp parallel for 
        for(i = 0; i< SIZE; i++) {
            field[i] = i%1024;

        }
 
}
int main (int argc, char *argv[])
{

    char*  fieldA;
    size_t i;
    char goal;
    int t;
    struct aclock timer;
    int threads = 1;
    if(argc >= 2){
        threads = atoi(argv[1]);
    }
     omp_set_num_threads(threads);

    init_aclock(&timer);
#ifdef HBW_MEM
    fieldA = memkind_malloc(MEMKIND_HBW, SIZE);
#else
    fieldA = malloc(SIZE);
#endif
    if(!fieldA){
    printf("could not allc, return \n");
    return(-1);
    }
    init(fieldA); 
    start_aclock(&timer);
    for(t = 0; t<ITER; t++) {
#pragma omp parallel for reduction(+ :goal)
        for(i = 0; i< SIZE; i++) {
            goal+=fieldA[i];

        }
    }
    end_aclock(&timer);
    printf("Timer %4d\t %f\n", threads,  get_seconds(&timer)/ITER);

}
