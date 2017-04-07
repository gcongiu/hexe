#include <stdio.h>
#include <stdlib.h>
#include <hexe.h>
#include <omp.h>

int main(void)
{
    double *field_A, *field_B, *field_C, *field_D;

    int is_in, verify;
    hexe_init();

    field_A = hexe_alloc_hbw((size_t)4* 1024*1024*1024);


    field_B = hexe_alloc_hbw_local((size_t)1024*1024*1024);


    field_C = hexe_alloc_ddr((size_t)4* 1024*1024*1024);


    field_D = hexe_alloc_ddr_local(1024* 1024*1024);

//    hexe_bind_requested_memory(0);



   is_in = hexe_is_in_hbw (field_A);
  verify  = hexe_verify_memory_region(field_A, (size_t) 4* 1024*1024*1024 , TOUCH_PAGES);
   printf("Hexe binds Field_A  to HBW? %d Verify? %s\n", is_in, (verify==0 ?  "yes": "no"));

    is_in = hexe_is_in_hbw (field_B);
    verify  = hexe_verify_memory_region(field_B, (size_t) 1024*1024*1024 , TOUCH_PAGES);
    printf("Hexe binds Field_B  to HBW? %d Verify? %s\n", is_in, (verify==0 ?  "yes": "no"));

    is_in = hexe_is_in_hbw (field_C);
    verify  = hexe_verify_memory_region(field_C, (size_t) 4* 1024*1024*1024 , TOUCH_PAGES);
    printf("Hexe binds Field_C  to HBW? %d Verify? %s\n", is_in, (verify==0 ?  "yes": "no"));

    is_in = hexe_is_in_hbw (field_D);
    verify  = hexe_verify_memory_region(field_D, (size_t) 1024*1024*1024 , TOUCH_PAGES);
    printf("Hexe binds Field_D  to HBW? %d Verify? %s\n", is_in, (verify==0 ?  "yes": "no"));


    hexe_free_memory(field_A);
    hexe_free_memory(field_B);
    hexe_free_memory(field_C);
    hexe_free_memory(field_D);


//    hexe_finalize();
}
