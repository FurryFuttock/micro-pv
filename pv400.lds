OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SECTIONS
{
  . = 0x0;              /* Start of the output file */

  _text = .;            /* Text and read-only data */

  .text : {
    *(.text)
  } = 0x9090

  _etext = .;           /* End of text section */

  /DISCARD/ : {
    *(.note.gnu.build-id)
  }

  .rodata : {           /* Read only data section */
    *(.rodata) 
    *(.rodata.*) 
  }

  .data : {             /* Data */
    *(.data)
  }

  _edata = .;           /* End of data section */
}
