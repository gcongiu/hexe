#include <numa.h>
#include <numaif.h>
#include "hexe_intern.h"
#define TOUCH_PAGES 0x1

static inline void hbw_touch_page(void* addr)
{
    volatile char* temp_ptr = (volatile char*) addr;
    char value = temp_ptr[0];
    temp_ptr[0] = value;
}


int hexe_verify_memory_region(void* addr, size_t size, int flags) {




    if (addr == NULL || size == 0 || flags & ~TOUCH_PAGES) {
        return EINVAL;
    }
    if(mem_manager->memory_mode == CACHE)
        return 0;
    /*
     * 4KB is the smallest pagesize. When pagesize is bigger, pages are verified more than once
     */
    const size_t page_size = sysconf(_SC_PAGESIZE);
    const size_t page_mask = ~(page_size-1);

    /*
     * block size should be power of two to enable compiler optimizations
     */
    const unsigned block_size = 64;

    char *end = addr + size;
    char *aligned_beg = (char*)((uintptr_t)addr & page_mask);
    hwloc_bitmap_t  expected_bitmap = mem_manager->all_mcdram;

    while(aligned_beg < end) {
        int nodes[block_size];
        void* pages[block_size];
        int i = 0, page_count = 0;
        char *iter_end = aligned_beg + block_size*page_size;

        if (iter_end > end) {
            iter_end = end;
        }

        while (aligned_beg < iter_end) {
            if (flags & TOUCH_PAGES) {
                hbw_touch_page(aligned_beg);
            }
            pages[page_count++] = aligned_beg;
            aligned_beg += page_size;
        }

        if (move_pages(0, page_count, pages, NULL, nodes, MPOL_MF_MOVE)) {
            return EFAULT;
        }

        for (i = 0; i < page_count; i++) {
            /*
             * negative value of nodes[i] indicates that move_pages could not establish
             * page location, e.g. addr is not pointing to valid virtual mapping
             */
            if(nodes[i] < 0) {
                return -1;
            }
            /*
             * if nodes[i] is not present in expected_nodemask then
             * physical memory backing page is not hbw
             */
            if (!hwloc_bitmap_isset(expected_bitmap, nodes[i])) {
            return -1;
            }
        }
    }

    return 0;
} 
