CFLAGS = -Wno-unknown-pragmas -Wall -g #-O3
LDFLAGS = -static #-O3

vmk: vmk.o preproc.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c preproc.h vlib/vstring.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f vmk *.o
