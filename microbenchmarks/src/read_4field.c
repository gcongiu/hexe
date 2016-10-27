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
#define ITER 16
#define SIZE (1024*1024*976)

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
	if(argc >= 2){
		threads = atoi(argv[1]);
	}
	omp_set_num_threads(threads);
	init_aclock(&timer);
#ifdef HBW_MEM
	fieldA = memkind_malloc(MEMKIND_HBW,2*sizeof(double)*(SIZE+3*1024*1024));
#else
	fieldA = malloc(2*sizeof(double)*(SIZE+3*1024*1024));
#endif
	fieldB = malloc(2*sizeof(double)*SIZE);

	init(fieldA); 
	init(fieldB);
	start_aclock(&timer);
	for(t = 0; t<ITER; t++) {
		for(k = 0; k< 2; k ++) {
#pragma omp parallel for reduction(+ :goal) firstprivate(k)
			for(i = 0; i< SIZE; i++) {
				goal+=fieldA[k*SIZE+i]+
				      fieldA[k*SIZE+i+1024*1024]+
				      fieldA[k*SIZE+i+2*1024*1024]+
				      fieldA[k*SIZE+i+3*1024*1024]+
				      fieldB[k*SIZE+i];

			}
		}
	}
	end_aclock(&timer);
	printf("Timer %d \t %f\n", threads,  get_seconds(&timer)/ITER);

}
