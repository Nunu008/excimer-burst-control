/* SPDX-License-Identifier: MIT
 *
 * Cortex-M7 image with the usual flash/RAM split. On MPS2 AN500 "flash" is the ZBT
 * SSRAM1 at 0x0 and RAM is SSRAM2/3 at 0x20000000; on an STM32H7 the same layout
 * maps to internal flash and DTCM by changing the two ORIGIN values.
 */
ENTRY(ebc_reset_handler)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 4M
  RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}

STACK_SIZE = 8K;

SECTIONS
{
  .isr_vector : ALIGN(128) { KEEP(*(.isr_vector)) } > FLASH

  .text : ALIGN(4)
  {
    *(.text .text.*)
    *(.glue_7 .glue_7t .vfp11_veneer .v4_bx)
  } > FLASH

  .rodata : ALIGN(4) { *(.rodata .rodata.*) } > FLASH

  .init_array : ALIGN(4)
  {
    __init_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
  } > FLASH

  .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > FLASH

  .data : ALIGN(8)
  {
    __data_start = .;
    *(.data .data.*)
    __data_end = .;
  } > RAM AT > FLASH
  __data_load = LOADADDR(.data);

  .bss (NOLOAD) : ALIGN(8)
  {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  } > RAM

  .stack (NOLOAD) : ALIGN(8)
  {
    . += STACK_SIZE;
    __stack_top = .;
  } > RAM
}
