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
    data = (float*) malloc (sizeof(float) * ELEMENTS);
    hexe_init();
    init(data);
    hexe_alloc_pool(sizeof(float)*1024, 2);
    hexe_start();

    hexe_start_fetch_strided(data, 1024, sizeof(float), sizeof(float)*1024, 0);
    cache =(float*) hexe_sync_fetch(0);
    printf("have here %f %f \t %f %f \n", cache[0], data[0], cache[1], data[1024]);
    cache = hexe_get_cache_for_write_back(1, &data[1024*1024]);
    cache[0] = 1024.434;
    cache[1] = 3324.434;
    hexe_start_write_back_strided(1024, sizeof(float), sizeof(float)*1024, 1);
    hexe_sync_fetch(1);

    printf("have here %f %f \t %f %f \n", cache[0], data[1024*1024], cache[1], data[1024*1024+1024]);
    hexe_finalize();

}
