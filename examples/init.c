#include <stdio.h>
#include <stdlib.h>
#include <hexe.h>
#include <omp.h>
int main(void)
{
    hexe_init();
    hexe_alloc_pool(4*1024*1024, 32);
    hexe_set_prefetch_threads(4);

    hexe_start();

    #pragma omp parallel 
    {
        int id = omp_get_thread_num();
     } 

    hexe_finalize();
}
