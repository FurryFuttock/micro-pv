#-- Scan directories to find what to compile
SRC_C=$(shell find . -iname '*.c')
SRC_ASM=$(shell find . -iname '*x86_64.S')
SRC_H=$(shell find . -iname '*.h')

#-- This is overkill, but safe. I have had problems generating .d files with $(CC) -MM in the past so
#-- we rebuild whenever ANY header file has changed or the Makefile or course
DEPS=Makefile $(SRC_H)

#-- Scan directories to get the ones that have include files
INC_DIR=$(shell find . -iname '*.h' | grep -o '.*/' | sort | uniq)
INC=$(foreach d,$(INC_DIR),-I$(d))

#-- Calculate what we want to build
OBJ_C=$(SRC_C:.c=.o)
OBJ_ASM=$(SRC_ASM:.S=.o)

#-- Set the Xen interface version. No idea where this version comes from, but if I don't set it then the whole thing falls over
XEN_INTERFACE_VERSION := 0x00040400

#-- Set compiler/assembler
CFLAGS  = -c -m64 -std=c99 -Wall -g -Dasm=__asm $(INC) -D__XEN_INTERFACE_VERSION__=$(XEN_INTERFACE_VERSION)
ASFLAGS = -c -m64 -D__ASSEMBLY__ $(INC)

#-- What we want to make
OUTPUT=pv400

#-- Make sure that we compile correctly
%.o : %.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $<

%.o : %.S $(DEPS)
	$(CC) $(ASFLAGS) -o $@ $<

#-- Rules
.PHONY: all
all : $(OUTPUT).gz

#-- Utility rules to type less when testing
start:
	if [ "`sudo xl list | grep $(OUTPUT)`" != "" ] ; then sudo xl destroy $(OUTPUT) ; fi
	sudo xl create -c $(OUTPUT).cfg

stop:
	if [ "`sudo xl list | grep $(OUTPUT)`" != "" ] ; then sudo xl destroy $(OUTPUT) ; fi

debug:
	sudo -b gdbsx -a `sudo xl domid pv400` 64 9999
	sleep 1
	gdb -x $(OUTPUT).gdb $(OUTPUT)

top dmesg list:
	sudo xl $@

#-- 1.- Merge all object files created by the compilation into a single relocatable object file
#-- 2.- Rewrite the object file keeping just the xenos_* and _start symbols as global
$(OUTPUT).o: $(OBJ_ASM) $(OBJ_C)
	$(LD) -r -m elf_x86_64 -o $@ $^
	objcopy -w -G xenos_* -G _start $@ $@

#-- Build the final absolute executable according to the linker file
$(OUTPUT) : $(OUTPUT).o $(OUTPUT).lds
	$(LD) -m elf_x86_64 -T $(OUTPUT).lds --cref -Map=$(OUTPUT).map -o $@ $<

#-- This pass is optional as Xen can load an elf file directly, however Mini-OS did it, so I do it as well
$(OUTPUT).gz : $(OUTPUT)
	gzip -f -9 -c $^ > $@

clean:
	rm -f $(OBJ_C) $(OBJ_ASM) $(OUTPUT).gz $(OUTPUT) $(OUTPUT).o $(OUTPUT).map
