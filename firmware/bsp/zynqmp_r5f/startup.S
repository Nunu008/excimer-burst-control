// SPDX-License-Identifier: MIT
//
// Exception vectors, reset and IRQ entry of the Zynq UltraScale+ RPU image
// (Cortex-R5F, ARMv7-R). Vectors live at 0x0 in the ATCM (SCTLR.V = 0) and are
// entered in ARM state (SCTLR.TE = 0); the C/C++ code they call is Thumb-2.

    .syntax unified
    .arm

    .equ MODE_IRQ, 0x12
    .equ MODE_SVC, 0x13
    .equ MODE_ABT, 0x17
    .equ MODE_UND, 0x1b
    .equ I_BIT,    0x80
    .equ F_BIT,    0x40

    .section .vectors, "ax"
    .global ebc_vectors
ebc_vectors:
    b       reset_entry
    b       fault_entry                 // undefined instruction
    b       fault_entry                 // SVC (unused: no RTOS, no semihosting via SVC here)
    b       fault_entry                 // prefetch abort
    b       fault_entry                 // data abort
    nop
    b       irq_entry
    b       fault_entry                 // FIQ (unused)

    .section .text.boot, "ax"
reset_entry:
    // One stack per exception mode we can enter, all in the BTCM.
    cps     #MODE_UND
    ldr     sp, =__und_stack_top
    cps     #MODE_ABT
    ldr     sp, =__abt_stack_top
    cps     #MODE_IRQ
    ldr     sp, =__irq_stack_top
    cps     #MODE_SVC
    ldr     sp, =__svc_stack_top

    // Low vectors (V = 0), exceptions in ARM state (TE = 0).
    mrc     p15, 0, r0, c1, c0, 0
    bic     r0, r0, #(1 << 13)
    bic     r0, r0, #(1 << 30)
    mcr     p15, 0, r0, c1, c0, 0
    isb

    // Enable the VFP: CPACR cp10/cp11 full access, then FPEXC.EN.
    mrc     p15, 0, r0, c1, c0, 2
    orr     r0, r0, #(0xf << 20)
    mcr     p15, 0, r0, c1, c0, 2
    isb
    mov     r0, #(1 << 30)
    vmsr    fpexc, r0

    ldr     r0, =ebc_c_entry            // crt.c (Thumb): .data/.bss, constructors, main()
    blx     r0
1:  b       1b

// IRQ entry. The handler is C++ and uses the FPU, so the caller-saved VFP state is
// part of the interrupt context. No nesting: the I bit stays set until return.
irq_entry:
    sub     lr, lr, #4
    push    {r0-r3, r12, lr}            // 24 bytes
    vpush   {s0-s15}                    // 64 bytes
    vmrs    r0, fpscr
    push    {r0, r1}                    // keeps the stack 8-byte aligned (AAPCS)
    ldr     r0, =ebc_irq_dispatch
    blx     r0
    pop     {r0, r1}
    vmsr    fpscr, r0
    vpop    {s0-s15}
    pop     {r0-r3, r12, lr}
    movs    pc, lr                      // return and restore CPSR from SPSR_irq

fault_entry:
    ldr     r0, =ebc_fault
    blx     r0
2:  b       2b
