CC = 	icc
CPP =  icpc
CFLAGS = -I/home/loden/host_libs/include

CFLAGS =   -O2 -xMIC-AVX512  -fast -static-intel  -restrict -fasm-blocks   \
		      -I/soft/perftools/tau/papi-knl/include\
			  -I/home/loden/host_libs/include\
			 -I/home/loden/hexe_c/include  -qopenmp 

CFLAGS  += -I../include/

LIBS =  -qopenmp -lpapi -L/soft/perftools/tau/papi-knl/lib/

CLINKFLAGS =	-O2 -fasm-blocks  -g -qopenmp -lnuma  -lhwloc -qopenmpi -L/home/loden/host_libs/lib/ 

default: bin/stencil_5pt.bin\
		bin/stencil_5pt_move_pages.bin\
		bin/stencil_5pt_copy.bin\
		bin/stencil_5pt_fetch.bin

obj/%.o: src/%.c ../libhexeknl.a ../include/hexe.h
	 $(CC) $(CFLAGS) -c -o $@ $<

obj/%_move_pages.o: src/%.c ../libhexeknl.a ../include/hexe.h
	 $(CC) $(CFLAGS) -DMOVE_PAGES -c -o $@ $<

obj/%_copy.o: src/%.c ../libhexeknl.a ../include/hexe.h
	 $(CC) $(CFLAGS) -DCOPY_DATA -c -o $@ $<

bin/%.bin: obj/%.o
	$(CC) $ $(CLINKFLAGS) -o $@ $<  ../libhexeknl.a  



