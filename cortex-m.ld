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

    /* ⚠️ THE TLS BLOCK, AND `__tls_base` IS WHAT A C LIBRARY LOOKS FOR. picolibc
     * reaches its `stdout` through thread-local storage, and the startup file
     * hands it this address. A script without these symbols links — they are
     * weak — and the program then faults on its first output with no
     * diagnostic. */
    .tdata : { __tls_base = .; *(.tdata*) } > RAM AT> FLASH
    .tbss (NOLOAD) : { __tbss_start = .; *(.tbss*) *(.tcommon) } > RAM

    /* ⚠️ FIVE SYMBOLS, NOT ONE. `_init_tls` copies the initialised block and
     * clears the zero one, and it reads every bound from the linker script —
     * there is nowhere else they could come from. A script that defines
     * `__tls_base` alone links (the startup file's declarations are weak) and
     * then fails on `__tdata_source` and three siblings. `PROVIDE` so a project
     * with its own script can define them differently without a clash. */
    PROVIDE(__tdata_source = LOADADDR(.tdata));
    PROVIDE(__tdata_size   = SIZEOF(.tdata));
    PROVIDE(__tbss_offset  = __tbss_start - __tls_base);
    PROVIDE(__tbss_size    = SIZEOF(.tbss));
    /* ⚠️ AND ONE MORE, WHICH IS THE ABI's RATHER THAN THE SCRIPT's. AAPCS puts
     * a two-word thread control block BELOW the thread pointer, so `_set_tls`
     * subtracts this offset before writing it. Upstream spells it
     * `MAX(8, __tls_align)`; eight is that value for every alignment a
     * Cortex-M block has. */
    PROVIDE(__arm32_tls_tcb_offset = 8);

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
