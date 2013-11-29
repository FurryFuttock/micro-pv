CFLAGS+=-g
LDFLAGS+=-g -lxenctrl -luuid

all: arinc653

arinc653 : arinc653.o

arinc653.o : arinc653.c

start : arinc653
	sudo ./arinc653 `sudo xl vm-list | grep pv400 | cut -d ' ' -f 1`

clean :
	rm -f *.o arinc653
