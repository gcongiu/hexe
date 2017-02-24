#include <numa.h>
#include <numaif.h>
#include "hexe_intern.h"
#include <sys/mman.h>
#include <errno.h>
struct node {
    void *addr;
     unsigned long long size;
    int priority;
    char location;
    struct node* next;
};

struct node *head = NULL;
struct node* pinned = NULL;
size_t n_elements = 0;
size_t new_elements = 0;
void insertList(void* addr, unsigned long long size, int priority) {

    struct node *link = (struct node*) malloc(sizeof(struct node));
    link->addr = addr;
    link->size = size;
    link->priority = priority;
    link->next = head;
    link->location = -1;

    head = link;
    n_elements++;
    new_elements++;

}


struct node* find(void* addr) {
    //start from the first link
    struct node* current = head;

    //if list is empty
    if(head == NULL) {
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
struct node* delete_from_list(void *addr){
    struct node * current = head;
    struct node *previous = NULL;

    if(current == NULL)
        return NULL;

    while((uint64_t)current->addr !=(uint64_t) addr) {
        //if it is last node
        if(current->next == NULL) {
            return NULL;
        } else {
            previous = current;
            current = current->next;
        }
    }
    if(!current)
        return NULL;
    //found
    if(current  ==  head){
        head = head->next;
    }
    else {
        //bypass
        previous->next = current->next;
    }
    if(current) {
        n_elements --;
        if(current->location == -1)
               new_elements -= 1;

    }
    return current;

}
/* function to swap data of two nodes a and b*/

void swap(struct node *a, struct node *b)
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
void sort_memory_list()
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


#define K(a,b)  K[(W+1)*(a) +(b)]
void  get_best_layout( unsigned long node_mask, unsigned long ddr_node_mask )
{
    char* is_in;
    int* K;
    int w, i;
    int W;
    size_t min;
    int ws[n_elements]; 
    struct node *current = head;

    size_t max_size = prefetcher->mcdram_memory;
    int mode = (prefetcher->mcdram_nodes > 1) ? MPOL_INTERLEAVE: MPOL_BIND;
    assert(head);
    min =  head->size;


    W = (size_t)max_size/min;

//    printf("W is %d \t \n", W);

    K = (int*)malloc((n_elements+1) * sizeof(int) *(W+1));
    is_in = malloc(n_elements * sizeof(char));
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
            if(current->location != 1)
             mbind(current->addr, current->size,    mode,
                        &node_mask, NUMA_NUM_NODES , MPOL_MF_MOVE );
         current->location = 1;
            prefetcher->mcdram_memory-=current->size;
        }
        else if(current->priority != 0) {
            if(current->location != 0)
                mbind(current->addr, current->size,  mode,
                        &ddr_node_mask, NUMA_NUM_NODES, MPOL_MF_MOVE);
            current->location = 0;
        }
        else
            current->location = -2; /*bind to all */

        madvise(current->addr, current->size, MADV_HUGEPAGE);
    current= current->next;
    }
    new_elements = 0;
    pinned = head;
 //   printf("have %d MB\n mcdram left\n", prefetcher->mcdram_memory/(1024*1024));
}


void print_list()
{

    struct node *ptr = head;
    while(ptr!= NULL) {
        printf("%d\t %ld \t %p \t %d \n", ptr->priority, ptr->size/(1024*1024), ptr->addr, ptr->location);
        ptr=ptr->next;

    }
}
