
#ifndef _Hexe_Allocator_hpp_
#define  _Hexe_Allocator_hpp_


#include <limits>
#include <iostream>
#include "hexe.h"

extern "C" {
  #include <pthread.h>
}


namespace hexe  
{



  class HexeDev {
    private:
        HexeDev() {
            if(!hexe_is_init())
                hexe_init();
            current_priority = 0;
        }
        ~HexeDev() {
            hexe_finalize();
        }
        int current_priority;
        static HexeDev* h_instance;
    public:
        int  distribute_objects(){
            return hexe_bind_requested_memory(0);
        }
        HexeDev *get_instance()
        {
            if(h_instance == 0)
                h_instance = new HexeDev(); 
            return h_instance;
        }

        int set_priority(int prio) {
          current_priority =prio;
        }
        int get_priority() {

        return current_priority;

        }

   };

  HexeDev *HexeDev::h_instance = 0;

    template <class T>
        class HexeAlloc {
                private:


            public:
                // type definitions
                typedef T        value_type;
                typedef T*       pointer;
                typedef const T* const_pointer;
                typedef T&       reference;
                typedef const T& const_reference;
                typedef std::size_t    size_type;
                typedef std::ptrdiff_t difference_type;

                int priority;

                HexeDev *hexe_p;
                // rebind allocator to type U
                template <class U>
                    struct rebind {
                        typedef HexeAlloc<U>  other;
                    };
                //     return address of  values 
                pointer address (reference value) const
                {
                    return  &value;
                }
                const_pointer  address (const_reference value) const
                {
                    return  &value;
                }

                HexeAlloc() throw() {
                     hexe_p->get_instance();
                }

                HexeAlloc(const HexeAlloc&) throw() {

                 }
                template <class U>
                    HexeAlloc (const HexeAlloc<U>&) throw() {
                    }
                ~HexeAlloc() throw() {
                }

                size_type max_size () const throw() {
                    return std::numeric_limits<std::size_t>::max() / sizeof(T);
                }
                // allocate but don't initialize num elements of type T
                pointer allocate (size_type num, const void* = 0) {
                    // print message and allocate memory with global new
                    uint64_t alloc_size = num * sizeof(T);
                    std::cout <<"allocate " << num << " elements with " << hexe_p->get_instance()->get_priority()<< " " <<alloc_size << "\n";
                    pointer ret = (pointer) hexe_request_hbw2((uint64_t)alloc_size, hexe_p->get_instance()->get_priority());

                    return ret;
                }

                int verify(size_type num, pointer P) {
                     return hexe_verify_memory_region((void*)P , num * sizeof (T) , 0x1);
                }

                          // initialize  elements of  allocated storage p with value value
                void construct (pointer  p, const  T& value)
                {
                    //  initialize  memory with lacement  new
                    new((void*)p)T(value);
                }

                //  destroy  elements  of initialized storage p
                void destroy (pointer p)
                {
                    p->~T();
                }

                // deallocate storage  of deleted  elements
                void deallocate (pointer  p, size_type num)
                {
                      hexe_free_memory(p);
                }


        };
}


hexe::HexeDev *h_p;

#define hexe_resize(prio, name, size) \
  h_p->get_instance()->set_priority(prio); \
  name.resize(size);

#define hexe_reserve(prio, name, size) \
  h_p->get_instance()->set_priority(prio); \
  name.reserve(size);

#define hexe_distribute_objects\
  h_p->get_instance()->distribute_objects(); 

#define hexe_verify(P)\
    P.get_allocator().verify(P.capacity(), (&P[0]))
//hexe::verify_region(&P[0], P.size())

#endif  //_Hexe_Allocator_hpp_

