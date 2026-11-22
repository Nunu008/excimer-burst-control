// SPDX-License-Identifier: MIT
//
// Reset entry of the Zynq UltraScale+ APU image (Cortex-A53, AArch64).
//
// Works at whatever exception level the boot flow hands over: EL3 straight from the
// FSBL or a JTAG load, EL2/EL1 behind ATF or in QEMU. Only core 0 runs the
// application; cores 1-3 are parked in WFE.

    .section .text.boot, "ax"
    .global _start
    .type _start, %function
_start:
    mrs     x0, mpidr_el1
    and     x0, x0, #0xff               // Aff0 = core number in the cluster
    cbnz    x0, park

    // Stop FP/SIMD from trapping at every level we may be running at. The control
    // law is floating point, so this must precede any C/C++ code.
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    cmp     x0, #3
    b.ne    1f
    msr     cptr_el3, xzr               // EL3: TFP = 0
1:  cmp     x0, #2
    b.lt    2f
    mov     x1, #0x33ff                 // CPTR_EL2 RES1 bits, TFP = 0
    msr     cptr_el2, x1
2:  mov     x1, #(3 << 20)              // CPACR_EL1.FPEN = 0b11
    msr     cpacr_el1, x1
    isb

    ldr     x1, =__stack_top
    mov     sp, x1
    bl      ebc_c_entry                 // crt.c: .data/.bss, constructors, main()

park:
    wfe
    b       park
    .size _start, . - _start
