#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "timer.h"
#include <omp.h>
#include <memkind.h>
#include <sys/mman.h>
#include<pthread.h>
#include <hwloc.h>
#define ITER 16
#define SIZE (1024*1024*640)

void init(double* field) {
	int i,k;

    for(k=0; k<2; k++)
#pragma omp parallel for 
		for(i = 0; i< SIZE; i++) {
			field[i+k*SIZE] = (double)i/1024.0;

		}

}
int main (int argc, char *argv[])
{

	double*  fieldA, *fieldB, *fieldC, *fieldD;
	size_t i,k;
	double goal;
	int t;
	struct aclock timer;
	int threads = 1;
	if(argc >= 2){
		threads = atoi(argv[1]);
	}
	omp_set_num_threads(threads);
	init_aclock(&timer);
#ifdef HBW_MEM
	fieldA = memkind_malloc(MEMKIND_HBW,sizeof(double)*2*(SIZE+2*1024*1024));
#else
	fieldA = malloc(sizeof(double)*2*(2*1024*1024+SIZE));
#endif
	fieldB = malloc(sizeof(double)*SIZE*4);
    init(fieldB);
    init(fieldB+2*SIZE);
	init(fieldA); 

    printf("memSIZE is %u \n", 2* (sizeof(double)*SIZE)/(1024*1024));
	start_aclock(&timer);
	for(t = 0; t<ITER; t++) {
		for(k = 0; k< 4; k ++) {
#pragma omp parallel for reduction(+ :goal) firstprivate(k)
			for(i = 0; i< SIZE; i++) {

                goal+=fieldB[k*SIZE+i]*(fieldA[i]+fieldA[i+SIZE]+fieldA[i+SIZE/2]);

			}
            goal= 0;
		}
	}
	end_aclock(&timer);
	printf("Timer %d \t %f\n", threads,  get_seconds(&timer)/ITER);

}
