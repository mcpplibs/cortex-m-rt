/* The memory map, and the two symbols a startup file cannot derive.
 *
 * ⚠️ THE VALUES BELOW ARE QEMU's `mps2-an385`, WHICH IS A DEFAULT AND NOT A
 * TRUTH. A real part has its own map and a project that targets one supplies
 * its own script — this file exists so that the FIRST program a newcomer builds
 * runs, not so that every program uses it.
 *
 * ⚠️⚠️ `KEEP(*(.vectors))` IS LOAD-BEARING. Nothing references the interrupt
 * vector table: the hardware reads it by address. mcpp links freestanding
 * targets with `--gc-sections` (since 2026.9.4.1), which is what keeps a C
 * library arriving from the dependency graph from putting its whole self into a
 * 64 KiB part — and which would collect the vector table with equal
 * enthusiasm. The criterion is that the image BOOTS, not that it links.
 */
ENTRY(Reset_Handler)

MEMORY {
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}

SECTIONS {
    .text : {
        KEEP(*(.vectors))
        *(.text.entry)
        *(.text*)
        *(.rodata*)
    } > FLASH

    .data : {
        __data_start = .;
        *(.data*)
        __data_end = .;
    } > RAM AT> FLASH
    __data_load = LOADADDR(.data);

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* One past the end of RAM: the initial stack pointer the hardware reads
     * out of vector slot 0 before it fetches a single instruction. */
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
