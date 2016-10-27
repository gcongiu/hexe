CC = 	icc
CPP = icpc
CFLAGS = -I/home/loden/host_libs/include



CFLAGS =   -O2 -xMIC-AVX512  -fast -static-intel  -restrict -fasm-blocks   \
		     -I/home/loden/knl_memkind/include/ -I/soft/perftools/tau/papi-knl/include\
			 -I/home/loden/hexe_c/include  -qopenmp 

LIBS = -L/home/loden/knl_memkind/lib/ -qopenmp -lpapi -L/soft/perftools/tau/papi-knl/lib

CLINKFLAGS =	-O2 -fasm-blocks  -g -lnuma  -lmemkind -lhwloc  -qopenmp /home/loden/hexe_c/libhexeknl.a
#CLINKFLAGS =	-O3 -fasm-blocks  -g -lnuma  -lhwloc -lmemkind


defaulte: bin/read_1time.bin bin/read_1time_hbw.bin\
		  bin/read_2times.bin bin/read_2times_hbw.bin\
		  bin/read_3times.bin bin/read_3times_hbw.bin\
   		  bin/read_2field.bin bin/read_2field_hbw.bin\
          bin/read_3field.bin bin/read_3field_hbw.bin\
          bin/read_4field.bin bin/read_4field_hbw.bin
write: bin/read_write1.bin bin/read_write1_hbw.bin \
       bin/read_write2.bin bin/read_write2_hbw.bin \
       bin/read_write3.bin bin/read_write3_hbw.bin \
       bin/write_read1_hbw.bin\
       bin/write_read2_hbw.bin\
       bin/write_read3_hbw.bin

large: bin/read_large.bin\
	   bin/read_large_cache.bin\
	   bin/read_large_2.bin\
	   bin/read_large_2_cache.bin\
	   bin/read_xlarge.bin\
       bin/read_xlarge_2.bin

random: bin/random_access.bin\
		bin/random_access_hbw.bin\
        bin/random_write.bin\
		bin/random_write_hbw.bin

prefetch: bin/read_2field_prefetch.bin\
		  bin/read_3field_prefetch.bin

basic: bin/copy_mcdram_ddr.bin bin/copy_ddr_mcdram.bin\
bin/write_hbw.bin bin/write.bin bin/read.bin bin/read_hbw.bin

stridet: bin/read_strided.bin \
	     bin/read_strided_hbw.bin\
		bin/write_strided.bin \
	     bin/write_strided_hbw.bin


papi: bin/read_write2_papi.bin bin/read_write2_hbw_papi.bin\
      bin/write_strided_papi.bin bin/write_strided_hbw_papi.bin bin/read_papi.bin
bin/%.o: src/%.c
	$(CC) $(CFLAGS) -c  -o $@ $<

bin/%_cache.o: src/%.c
	$(CC) $(CFLAGS) -DCACHE -c  -o $@ $<

bin/%_papi.o: src/%.c
	$(CC) $(CFLAGS) -DUSE_PAPI -c  -o $@ $<

bin/%_hbw_papi.o: src/%.c
	$(CC) $(CFLAGS) -DUSE_PAPI  -DHBW_MEM -c  -o $@ $<


bin/%_hbw.o: src/%.c
	$(CC) -DHBW_MEM $(CFLAGS) -c  -o $@ $<

bin/%_prefech.bin: bin/%_prefetch_.o
	$(CPP) $(LINKFLAGS) -o $@ $(LIBS) $<


bin/%.bin: bin/%.o
	$(CPP) $(CLINKFLAGS) -o $@ $(LIBS) $<


clean:
	rm -f bin/*


