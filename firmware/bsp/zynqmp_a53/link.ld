/* SPDX-License-Identifier: MIT
 *
 * Zynq UltraScale+ APU image in PS DDR (low 2 GiB window). Loaded as an ELF by the
 * FSBL, by JTAG (xsdb) or by QEMU's generic loader; load address == run address.
 */
ENTRY(_start)

MEMORY
{
  DDR (rwx) : ORIGIN = 0x00100000, LENGTH = 16M
}

STACK_SIZE = 64K;

SECTIONS
{
  .text : ALIGN(64)
  {
    KEEP(*(.text.boot))
    *(.text .text.*)
  } > DDR

  .rodata : ALIGN(16) { *(.rodata .rodata.*) } > DDR

  .init_array : ALIGN(8)
  {
    __init_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
  } > DDR

  .data : ALIGN(16)
  {
    __data_start = .;
    *(.data .data.*)
    __data_end = .;
  } > DDR
  __data_load = LOADADDR(.data);

  .bss (NOLOAD) : ALIGN(16)
  {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  } > DDR

  .stack (NOLOAD) : ALIGN(16)
  {
    . += STACK_SIZE;
    __stack_top = .;
  } > DDR

  /DISCARD/ : { *(.ARM.exidx*) *(.eh_frame*) }
}
