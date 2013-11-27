CC         = gcc
ARCH       = $(shell uname -m)
STACK_SIZE = 0x1000
PAGE_SIZE  = $(shell getconf PAGESIZE)
ifeq ($(ARCH), x86_64)
CPPFLAGS += -m64 -g -I./include/x86
endif
CFLAGS   += -std=c99 -I./include -D__PAGE_SIZE=$(PAGE_SIZE) -D__STACK_SIZE=$(STACK_SIZE)
ASFLAGS   = -D__ASSEMBLY__ -D__PAGE_SIZE=$(PAGE_SIZE) -D__STACK_SIZE=$(STACK_SIZE)

.PHONY: all

all: libstdlib.a

SRC_ASM=$(wildcard *.$(ARCH).S)
SRC_C=$(wildcard *.c)

OBJ_ASM=$(SRC_ASM:.S=.o)
OBJ_C=$(SRC_C:.c=.o)

OBJ=$(OBJ_ASM) $(OBJ_C) $(OBJ_CPP)
DEP=$(OBJ:.o=.d)

# pull in dependency info for *existing* .o files
-include $(DEP)

# compile and generate dependency info
%.d : %.S
	$(CC) -MM $(ASFLAGS) -o $@ $<

%.d : %.c
	$(CC) -MM $(CFLAGS) $(CPPFLAGS) -o $@ $<

libstdlib.a : $(DEP) $(OBJ)
	$(AR) -cr $@ $(OBJ)

clean:
	rm -f libxen_guest.a $(OBJ) $(DEP)
