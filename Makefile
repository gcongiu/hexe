CC = 	icc
CPP =  icpc
CFLAGS = -I/home/loden/host_libs/include

default: libhexeknl.a\
		 lib/libhexe.so\
		 libhexemalloc.a\
		 lib/libhexemalloc.so

example: examples/init.bin\
		 examples/allocate.bin\
		 examples/allocate2.bin\
		 examples/start_sync.bin\
		 examples/start_sync_strided.bin\
		 examples/stl.bin


all: default  example

OBJ = obj/hexe_omp.o obj/verify_omp.o

OBJ_MALLOC = obj/hexe.o obj/verify.o


clean:
	rm -rf obj/* examples/*.bin

CFLAGS =   -O3  -xMIC-AVX512 -ipo  -static-intel  -restrict -fasm-blocks   \
		      -I/soft/perftools/tau/papi-knl/include\
	          -qopenmp 

CFLAGS_MALLOC =  -O3 -xMIC-AVX512 -ipo  -restrict -fasm-blocks -I./src -I./include 

CFLAGS  += -I./include/ -I./src 

LIBS =  -qopenmp -lpapi  -lhwloc -parallel -L/soft/perftools/tau/papi-knl/lib/

CLINKFLAGS =	-O3 -xMIC-AVX512 -restrict -fasm-blocks  -g -ipo

lib/libhexe.so: $(OBJ) 
	$(CC) -shared -Wl,-soname,libhexe.so.1  -o $@  $(OBJ) -lc

lib/libhexemalloc.so: $(OBJ_MALLOC)  include/list.h include/hexe_malloc.h 
	$(CC) -shared -Wl,-soname,libhexemalloc.so.1  -o $@  $(OBJ_MALLOC) -lc

obj/%_omp.o: src/%.c src/prefetch.h include/list.h include/hexe_malloc.h
	 $(CC) -fPIC -DWITH_PREFETCHER $(CFLAGS) -c -o $@ $<

obj/%.o: src/%.c include/list.h include/list.h include/hexe_malloc.h
	$(CC) -fPIC $(CFLAGS_MALLOC) -c -o $@ $<

examples/%.bin: examples/%.o  
	$(CC) $ $(CLINKFLAGS) $(LIBS)  -o $@ $<  libhexeknl.a  

examples/%.o: examples/%.c libhexeknl.a
	$(CC) $ $(CFLAGS) -c -o $@ $<

examples/%.bin: examples/%.opp
	$(CPP) $ $(CLINKFLAGS) $(LIBS) -o $@ $<  libhexeknl.a

examples/%.opp: examples/%.cpp libhexeknl.a
	$(CPP) $ $(CFLAGS) -c -o $@ $<

libhexeknl.a: $(OBJ)
	ar cvr libhexeknl.a  $(OBJ)

libhexemalloc.a: $(OBJ_MALLOC)
	ar cvr libhexemalloc.a  $(OBJ_MALLOC)


