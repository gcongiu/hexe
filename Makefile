CC = 	icc
CFLAGS = -I/home/loden/host_libs/include

default: libhexeknl.a

example: examples/init.bin\
		 examples/start_sync.bin\
		 examples/start_sync_strided.bin

all: default  example

SRC = src/hexe.c\
	 src/prefetch.c
INC = include/hexe.h\
  	  include/prefetch.h

OBJ = obj/hexe.o 

clean:
	rm -rf obj/* examples/*.bin

CFLAGS =   -O2 -xMIC-AVX512  -fast -static-intel  -restrict -fasm-blocks   \
		      -I/soft/perftools/tau/papi-knl/include\
			  -I/home/loden/host_libs/include\
			 -I/home/loden/hexe_c/include  -qopenmp 

CFLAGS  += -I./include/ -I./src

LIBS =  -qopenmp -lpapi -L/soft/perftools/tau/papi-knl/lib/

CLINKFLAGS =	-O2 -fasm-blocks  -g -qopenmp -lnuma  -lhwloc -qopenmpi -L/home/loden/host_libs/lib/ 

obj/%.o: src/%.c src/prefetch.h
	 $(CC) $(CFLAGS) -c -o $@ $<

examples/%.bin: examples/%.o  
	$(CC) $ $(CLINKFLAGS) -o $@ $<  libhexeknl.a  

examples/%.o: examples/%.c libhexeknl.a
	$(CC) $ $(CFLAGS) -c -o $@ $<

libhexeknl.a: $(OBJ) 
	ar cvr libhexeknl.a  $(OBJ)


