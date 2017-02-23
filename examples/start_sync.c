#include <stdio.h>
#include <stdlib.h>
#include <hexe.h>
#include <omp.h>
#define ELEMENTS (128*1024*1024)
void init( float *field)
{
    int i;
    for (i = 0; i < ELEMENTS; i++) {
        field[i] = 0.2324 * i + 32.3/(i+1);
    }
}

int main(void)
{
    float *data, *cache;
    int i;
    data = (float*) malloc (sizeof(float) * ELEMENTS);
    hexe_init();
    init(data);
    hexe_alloc_pool(sizeof(float)*1024, 2);

    float *field_A = hexe_request_hbw(NULL, 1024*1024*1024, 2);
    hexe_start();

    hexe_start_fetch_continous(data, 1024*sizeof(float), 0);
    #pragma omp parallel for 
    for (i = 0; i< 1024*1024 *128; i++) {
        field_A[i] = (float) i/2.34;

    }
    cache =(float*) hexe_sync_fetch(0);
    printf("have here %f %f %f \n", cache[0], data[0], field_A[12]);
    cache = hexe_get_cache_for_write_back(1, &data[1024]);
    cache[0] = 1024.434;

    hexe_start_write_back_continous(1024*sizeof(float),1);
    hexe_sync_fetch(1);

    printf("have here %f %f \n", cache[0], data[1024]);
    hexe_finalize();

}
