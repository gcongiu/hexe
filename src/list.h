struct node {
    void *addr;
    size_t size;
    int priority;
    int w_p;
    struct node* next;
};

struct node *head = NULL;
size_t n_elements = 0;
void insertList(void* addr, size_t size, int priority) {

    struct node *link = (struct node*) malloc(sizeof(struct node));
    link->addr = addr;
    link->size = size;
    link->priority = priority;
    link->next = head;
    head = link;
    n_elements++;

}

//delete a link with given key
struct node* find(void* addr) {
    //start from the first link
    struct node* current = head;
    struct node* previous = NULL;

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
            previous = current;
            //move to  next link
            current = current->next;
        }
    }
    return current;
}
void delete (void *addr){
    struct node * current = NULL;
    struct node* previous = NULL;

    current = find(addr);
    if(!current)
        return; 
    //found
    if(current  ==  head){
        head = head->next;
    }
    else {
        //bypass
        previous->next = current->next;
    }
    n_elements--;
    return;

}
/* function to swap data of two nodes a and b*/

void swap(struct node *a, struct node *b)
{
    struct node tmp;
    tmp.addr = a->addr;
    tmp.size = a->size;
    tmp.priority = a->priority;

    a->addr = b->addr;
    a->size = b->size;
    a->priority = b->priority;

    b->addr = tmp.addr;
    b->size = tmp.size;
    b->priority = tmp.priority;


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
        while (ptr1->next != NULL)
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


#define K(a,b)  K[(W+1)*(a) +(b)]
void  get_best_layout(size_t max_size, int fast_nodes, unsigned long node_mask )
{
    char* is_in;
    int* K;
    int w, i;
    int W;
    int min = head->size;
    int ws[n_elements]; 
    struct node *current = head;

    W = (size_t)max_size/min;



    K = (int*)malloc((n_elements+1) * sizeof(int) *(W+1));
    is_in = malloc(n_elements * sizeof(char));
    for (w =0; w<=W; w++)
        K(0,w) = 0;

    for (i = 1; i <= n_elements; i++)
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
        printf("\n");
    }
    i = n_elements;
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
    for(i = 0; i<n_elements; i++) {
        printf("is in? %p %d\n", current->addr, is_in[i]);
        if(is_in[i] == 1)
        {
        mbind(current->addr, current->size,  MPOL_INTERLEAVE,
          &node_mask, fast_nodes, MPOL_MF_MOVE);


        }
        current= current->next;
    }

}


void print_list()
{

    struct node *ptr = head;
    while(ptr!= NULL) {
        printf("%d\t %ld \t %p \n", ptr->priority, ptr->size/(1024*1024), ptr->addr);
        ptr=ptr->next;

    }
}
