CC = 	icc
CFLAGS = -I/home/loden/host_libs/include

default: libhexeknl.a

SRC = src/hexe.c\
	 src/prefetch.c
INC = include/hexe.h\
  	  include/prefetch.h

OBJ = obj/hexe.o obj/prefetch.o

clean:
	rm -rf obj/* 



CFLAGS =   -O3 -xMIC-AVX512  -fast -static-intel  -restrict -fasm-blocks   \
		     -I/home/loden/knl_memkind/include/  -qopenmp
CFLAGS  += -I./include/
LD_FLAGS = -qopenmp 


obj/%.o: src/%.c
	 $(CC) $(CFLAGS) -c -o $@ $<


libhexeknl.a: $(OBJ) /home/loden/knl_memkind/lib/libmemkind.a
	ar cvr libhexeknl.a  $(OBJ) /home/loden/knl_memkind/lib/libmemkind.a

clean:
	rm -rf obj/* .lib/
