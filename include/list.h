#ifndef  LIST_H
#define LIST_H
#include <numa.h>
#include <numaif.h>
#define _GNU_SOURCE   
#include <sys/mman.h>
#include <errno.h>
enum memory_location{
    UNDEFINED = -1,
    MCDRAM_KNP = 0,
    DDR_KNP = 1,
    MCDRAM_FIXED = 2,
    DDR_FIXED = 3,
    INTERLEAVED = 4
};


struct node {
    void *addr;
     unsigned long long size;
    int priority;
    int  location;
    struct node* next;
};

extern struct node *head;
extern struct node* pinned;
extern struct node* fixed;
static size_t n_elements = 0;
static size_t new_elements = 0;

static inline struct node* insertlist_fixed(void* addr, unsigned long long size, int location) {

    struct node *link = (struct node*) malloc(sizeof(struct node));
    link->addr = addr;
    link->size = size;
    link->priority = -0xFF;
    link->next = fixed;
    link->location = location;
    fixed = link;
    return link;
}



static inline struct node* insertList(void* addr, unsigned long long size, int priority) {

    struct node *link = (struct node*) malloc(sizeof(struct node));
    link->addr = addr;
    link->size = size;
    link->priority = priority;
    link->next = head;
    link->location = UNDEFINED;

    head = link;
    n_elements++;
    new_elements++;
    return link;
}


static inline struct node* find(void* addr,int flag) {
    //start from the first link
    //
    struct node* current = head;
    if(flag == 1)
            current = fixed;
    //if list is empty
    if(current == NULL) {
        return NULL;
    }

    //navigate through list
    while((uint64_t)current->addr !=(uint64_t) addr) {
        //if it is last node
        if(current->next == NULL) {
            return NULL;
        } else {
            //store reference to current
            //move to  next link
            current = current->next;
        }
    }
    return current;
}
static inline struct node* delete_from_list(void *addr){
    struct node * current = head;
    struct node *previous = NULL;

    if(current == NULL)
        goto look_fixed;

    while((uint64_t)current->addr !=(uint64_t) addr) {
        //if it is last node
        if(current->next == NULL) {
            current = NULL;
            break;
        } else {
            previous = current;
            current = current->next;
        }
    }


look_fixed:
    if(!current) {
        if(! fixed) return NULL;

        current = fixed;
        while((uint64_t)current->addr !=(uint64_t) addr) {
            //if it is last node
            if(current->next == NULL) {
                return NULL;
            } else {
                previous = current;
                current = current->next;
            }
        }

    }
 /*look in the fixed list */

    //found
    if(current  ==  head){
        head = head->next;
    }
    else if (current == fixed)
        fixed = fixed->next;
    else {
        //bypass
        previous->next = current->next;
    }
    if(current) {
        if(current->location != MCDRAM_FIXED && current->location != DDR_FIXED)
            n_elements --;
        if(current->location == -1)
               new_elements -= 1;

    }
    return current;

}
/* function to swap data of two nodes a and b*/

static inline void swap(struct node *a, struct node *b)
{
    struct node tmp;

    tmp.addr = a->addr;
    tmp.size = a->size;
    tmp.priority = a->priority;
    tmp.location = a->location;

    a->addr = b->addr;
    a->size = b->size;
    a->priority = b->priority;
    a->location = b->location;

    b->addr = tmp.addr;
    b->size = tmp.size;
    b->priority = tmp.priority;
    b->location = tmp.location;
}
/* Bubble sort the given linked lsit */
static inline  void sort_memory_list()
{
    int swapped, i;
    struct node *ptr1 = head;
    struct node *lptr = NULL;

    /* Checking for empty list */
    if (ptr1 == NULL)
        return;

    do
    {
        swapped = 0;
        ptr1 = head;
        while (ptr1->next != lptr && ptr1->next !=pinned)
        {
            if (ptr1->size > ptr1->next->size)
            {
                swap(ptr1, ptr1->next);
                swapped = 1;
            }
            ptr1 = ptr1->next;
        }
        lptr = ptr1;
    }
    while (swapped);
}


static inline int max (int a, int b)
{
    return a > b ? a:b;
}

static void reset_list()
{
    pinned = NULL;
    new_elements = n_elements;
}

static inline int bind_strided( void *addr, size_t size, int flag)
{
    int n_nodes, i;
    size_t chunk_size;
    char* current_addr = (char*) addr;
    hwloc_membind_policy_t  mode = (flag> 1) ? HWLOC_MEMBIND_INTERLEAVE: HWLOC_MEMBIND_BIND;
    hwloc_nodeset_t nodeset = hwloc_bitmap_alloc();
    int ret;
    n_nodes = mem_manager->ddr_nodes;
    chunk_size = size/n_nodes;


    if(chunk_size & 0xfff) {
        chunk_size =  (chunk_size+4096) & ~((unsigned long)(0xfff));
    }



    for(i = 0; i < n_nodes; i++) {
        if(flag == 0)
             hwloc_bitmap_copy(nodeset, mem_manager->mcdram_sets[i]);
        else if (flag == 1) 
             hwloc_bitmap_copy(nodeset,mem_manager->ddr_sets[i]);
        else
             hwloc_bitmap_or(nodeset, mem_manager->mcdram_sets[i],  mem_manager->ddr_sets[i]);

            ret = hwloc_set_area_membind_nodeset(mem_manager->topology, current_addr, chunk_size,
                           nodeset, mode,  HWLOC_MEMBIND_MIGRATE);

        current_addr += chunk_size;
    }
    hwloc_bitmap_free(nodeset);
    return ret;

 }


#define K(a,b)  K[(W+1)*(a) +(b)]
static inline void  get_best_layout()
{
    char* is_in;
    int* K;
    int w, i;
    int W;
    size_t min;
    int ws[n_elements];
    struct node *current = head;

    size_t max_size = mem_manager->mcdram_avail;

    hwloc_membind_policy_t  mode = (mem_manager->ddr_nodes> 1) ? HWLOC_MEMBIND_INTERLEAVE: HWLOC_MEMBIND_BIND;
    assert(head);
    min =  head->size;

    W = (size_t)max_size/min;

    K = (int*)malloc((n_elements+1) * sizeof(int) *(W+1));
    is_in =(char*) malloc(n_elements * sizeof(char));
    for (w =0; w<=W; w++)
        K(0,w) = 0;

    for (i = 1; i <= new_elements; i++)
    {
        //   current = head;
        int tmp_size =current->size/min;
        ws[i]=tmp_size;

        for (w = 0; w <= W; w+=1)
        {
            if(w==0)
                K(i,w) = 0;
            else if (tmp_size <= w)
                K(i,w) = max(current->priority + K((i-1),(w-tmp_size)), K((i-1),w));
            else
                K(i,w) = K((i-1),w);
        }
        current = current->next;
    }
    i = new_elements;
    w = W;
    while( i> 0 && w >0) {
        if(K(i,w) != K((i-1),w)) {
            is_in[i-1] = 1;
            w = w-ws[i];
            i = i-1;
        }
        else{
            is_in[i-1]=0;
            i = i-1;
        }
    }
    current = head;
    for(i = 0; i<new_elements; i++) {
        errno  = 0;
        if(is_in[i] == 1)
        {
            if((current->location != MCDRAM_KNP ))
               if( (mem_manager->mcdram_nodes > 1) ) 
                    bind_strided( current->addr, current->size, 0);
                else
                    hwloc_set_area_membind_nodeset(mem_manager->topology, current->addr, current->size,
                           mem_manager->all_mcdram, mode,  HWLOC_MEMBIND_MIGRATE|HWLOC_MEMBIND_STRICT);
            current->location = MCDRAM_KNP;
            mem_manager->mcdram_avail-=current->size;
        }
        else if(current->priority != 0) {
            if((current->location != DDR_KNP) )
                hwloc_set_area_membind_nodeset(mem_manager->topology, current->addr, current->size,
                              mem_manager->all_ddr, mode,  HWLOC_MEMBIND_MIGRATE);
            current->location = DDR_KNP;
        }
        else
            current->location = UNDEFINED; /*bind to all */

        madvise(current->addr, current->size, MADV_HUGEPAGE);
        current= current->next;
    }
    new_elements = 0;
    pinned = head;
    free(K);
    free(is_in);
    //   printf("have %d MB\n mcdram left\n", mem_manager->mcdram_avail/(1024*1024));
}


static void print_list()
{

    struct node *ptr = head;
    while(ptr!= NULL) {
        printf("%d\t %ld \t %p \t %d \n", ptr->priority, ptr->size/(1024*1024), ptr->addr, ptr->location);
        ptr=ptr->next;

    }
}

#endif
