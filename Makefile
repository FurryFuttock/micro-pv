#-- source directories
SRC_DIRS=stdlib src

#-- Scan directories to find what to compile
SRC_C=$(foreach d,$(SRC_DIRS),$(shell find $(d) -iname '*.c'))
SRC_ASM=$(foreach d,$(SRC_DIRS),$(shell find $(d) -iname '*x86_64.S'))
SRC_H=$(foreach d,$(SRC_DIRS),$(shell find $(d) -iname '*.h')) micropv.h

#-- This is overkill, but safe. I have had problems generating .d files with $(CC) -MM in the past so
#-- we rebuild whenever ANY header file has changed or the Makefile or course
DEPS=Makefile $(SRC_H)

#-- Scan directories to get the ones that have include files
INC_DIRS=$(foreach d,$(SRC_DIRS),$(shell find $(d) -iname '*.h' | grep -o '.*/' | sort | uniq))
INC_FLAGS=$(foreach d,$(INC_DIRS),-I$(d))

#-- Calculate what we want to build
OBJ_C=$(SRC_C:.c=.o)
OBJ_ASM=$(SRC_ASM:.S=.o)

#-- Set the Xen interface version. No idea where this version comes from, but if I don't set it then the whole thing falls over
XEN_INTERFACE_VERSION := 0x00040400

#-- Set compiler/assembler
#CFLAGS  = -c -m64 -std=c99 -Wall -g $(INC_FLAGS) -D__XEN_INTERFACE_VERSION__=$(XEN_INTERFACE_VERSION) -mno-red-zone -fno-builtin -fno-stack-protector -fgnu89-inline -fno-reorder-blocks -fno-asynchronous-unwind-tables
CFLAGS  = -c -m64 -std=c99 -Wall -g $(INC_FLAGS) -D__XEN_INTERFACE_VERSION__=$(XEN_INTERFACE_VERSION) -mno-red-zone
ASFLAGS = -c -m64 -D__ASSEMBLY__ $(INC_FLAGS)

#-- What we want to make
OUTPUT=micropv

#-- Make sure that we compile correctly
%.o : %.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $<

%.o : %.S $(DEPS)
	$(CC) $(ASFLAGS) -o $@ $<

#-- Rules
.PHONY: all
all : $(OUTPUT).o

#-- 1.- Merge all object files created by the compilation into a single relocatable object file
#-- 2.- Rewrite the object file keeping the minimum symbols as global. This doesn't affect debugging as the debug info is not removed.
$(OUTPUT).o: $(OBJ_ASM) $(OBJ_C)
	$(LD) -r -m elf_x86_64 -Map=$(OUTPUT).map -o $@ $^
	objcopy -w -G _start -G do_exit -G micropv_* -G printk -G stack $@ $@

clean:
	rm -f $(OBJ_C) $(OBJ_ASM) $(OUTPUT).o $(OUTPUT).map
