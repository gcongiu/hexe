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

    size_t page_size = sysconf(_SC_PAGESIZE);
    const size_t page_mask = ~(page_size-1);
    char *aligned_beg = (char*)((uintptr_t)addr);
    hwloc_bitmap_t  expected_bitmap = mem_manager->all_mcdram; 
    hwloc_bitmap_t location;
    hwloc_membind_policy_t policy;
    hwloc_bitmap_t and_map = hwloc_bitmap_alloc();
   if ( hwloc_get_area_membind_nodeset(mem_manager->topology, aligned_beg, size, location , &policy, HWLOC_MEMBIND_STRICT)) {
            return EFAULT;
     }
    printf("here %x\n", hwloc_bitmap_to_ulong(location));
     hwloc_bitmap_and(and_map, location, expected_bitmap);
 
  if(hwloc_bitmap_iszero(and_map))
        return -1;
 
    return 0;
}
