XEN_INTERFACE_VERSION := 0x00040400
CFLAGS+=-g -D__XEN_INTERFACE_VERSION__=$(XEN_INTERFACE_VERSION)
LDFLAGS+=-g -lxenctrl -luuid

all: arinc653

arinc653 : arinc653.o

arinc653.o : arinc653.c

start : arinc653
	-sudo ./arinc653 `sudo xl vm-list | grep pv400 | cut -d ' ' -f 1`
	-sudo ./arinc653 "00000000-0000-0000-0000-000000000000"

debug :
	sudo gdb ./arinc653

clean :
	rm -f *.o arinc653
