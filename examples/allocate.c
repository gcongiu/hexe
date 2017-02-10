#include <stdio.h>
#include <stdlib.h>
#include <hexe.h>
#include <omp.h>
int main(void)
{
    double *field_A, *field_B, *field_C, *field_D;
    hexe_init();
    hexe_alloc_pool(4*1024*1024, 32);
    hexe_set_prefetch_threads(4);
    hexe_set_compute_threads(8);
    
    hexe_start();
    field_A = hexe_request_hbw( (size_t)4* 1024*1024*1024, 2);
    field_B = hexe_request_hbw((size_t)8*1024*1024*1024, 4);
    field_C = hexe_request_hbw((size_t)7* 1024*1024*1024, 1);
    field_D = hexe_request_hbw(1024*1024*1024, 8);
    hexe_bind_requested_memory();
    hexe_finalize();

}
