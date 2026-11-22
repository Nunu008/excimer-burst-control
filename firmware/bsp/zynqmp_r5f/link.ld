/* SPDX-License-Identifier: MIT
 *
 * Zynq UltraScale+ RPU, one Cortex-R5F in split mode: 64 KiB ATCM at 0x0 holds the
 * vectors and all code, 64 KiB BTCM at 0x20000 holds data, the learning tables and
 * the stacks. Nothing in the pulse interrupt touches DDR or a cache, so its timing
 * does not depend on what the APU is doing on the memory bus.
 */
ENTRY(ebc_vectors)

MEMORY
{
  ATCM (rx)  : ORIGIN = 0x00000000, LENGTH = 64K
  BTCM (rw)  : ORIGIN = 0x00020000, LENGTH = 64K
}

SVC_STACK_SIZE = 4K;
IRQ_STACK_SIZE = 2K;
ABT_STACK_SIZE = 256;
UND_STACK_SIZE = 256;

SECTIONS
{
  .vectors 0x0 : { KEEP(*(.vectors)) } > ATCM

  .text : ALIGN(8)
  {
    KEEP(*(.text.boot))
    *(.text .text.*)
    *(.glue_7 .glue_7t .vfp11_veneer .v4_bx)
  } > ATCM

  .rodata : ALIGN(8) { *(.rodata .rodata.*) } > ATCM

  .init_array : ALIGN(4)
  {
    __init_array_start = .;
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
  } > ATCM

  .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > ATCM

  .data : ALIGN(8)
  {
    __data_start = .;
    *(.data .data.*)
    __data_end = .;
  } > BTCM AT > ATCM
  __data_load = LOADADDR(.data);

  .bss (NOLOAD) : ALIGN(8)
  {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  } > BTCM

  .stacks (NOLOAD) : ALIGN(8)
  {
    . += SVC_STACK_SIZE; __svc_stack_top = .;
    . += IRQ_STACK_SIZE; __irq_stack_top = .;
    . += ABT_STACK_SIZE; __abt_stack_top = .;
    . += UND_STACK_SIZE; __und_stack_top = .;
  } > BTCM
}
