/* The vector table and reset path — the two things no program on this machine
 * can supply for itself.
 *
 * ⚠️ C RATHER THAN C++, AND THAT IS THE ONE PLACE IT MATTERS. This translation
 * unit runs BEFORE `.data` is copied and `.bss` is cleared, so it must not read
 * a global with an initialiser or write one that lives in either section. C
 * makes that easy to see; a C++ file in the same position invites a static
 * constructor that the runtime has not yet been able to run.
 *
 * ⚠️⚠️ THE TABLE IS REFERENCED BY NOTHING. The hardware reads it at the image's
 * load address, and mcpp links freestanding targets with `--gc-sections`, so it
 * survives only because the linker script says `KEEP(*(.vectors))`. The
 * criterion for this file is that the image BOOTS, never that it links.
 */
extern unsigned __stack_top;
extern unsigned __data_start, __data_end, __data_load;
extern unsigned __bss_start, __bss_end;

/* ⚠️⚠️ `main` IS DECLARED HERE AS C, AND A C++ PROGRAM'S `main` IS NOT.
 *
 * A C++ `main` at global scope is exempt from mangling only when the compiler
 * treats it as THE program entry point, and for a freestanding target it does
 * not: measured, `int main()` in a `.cpp` compiled for `thumbv7m-none-eabi`
 * emits `_Z4mainv`, and this file's `extern "C"`-shaped reference to `main`
 * then fails to link with clang helpfully asking whether `main` should be
 * `extern "C"`.
 *
 * The startup file cannot change the program's spelling, so the ENTRY POINT
 * gets a name of its own. `board_main` is what this package calls; the
 * `[[gnu::alias]]`-free shim in `cortex_m_rt.cppm` is what lets a program keep
 * writing `int main()`. */
int  board_main(void);
void Reset_Handler(void);

/* Supplied by a C library when one is in the graph; the weak definitions below
 * are what the zero-libc tier links instead. `__tls_base` is the linker
 * script's, and is zero when no C library asked for a TLS block. */
extern char __tls_base[] __attribute__((weak));
__attribute__((weak)) void _init_tls(void* p)      { (void)p; }
__attribute__((weak)) void _set_tls(void* p)       { (void)p; }
__attribute__((weak)) void __libc_init_array(void) { }

/* A handler nothing overrides. It spins rather than resetting: a fault that
 * silently restarts the machine is the hardest kind of bug to see, and this is
 * the state a debugger can actually be attached to. */
__attribute__((weak)) void Default_Handler(void) { for (;;) {} }

void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__((weak, alias("Default_Handler")));

/* ⭐ EVERY SLOT IS `weak`, WHICH IS HOW A PROJECT TAKES ONE WITHOUT EDITING
 * THIS FILE. Defining `SysTick_Handler` anywhere in the program replaces the
 * spin; defining nothing leaves a fault visible instead of silent. That is also
 * how a scheduler built on `openarch` installs its own PendSV: it names
 * `openarch_cm_pendsv` and the linker prefers the strong definition. */
__attribute__((section(".vectors"), used))
void (* const g_vectors[])(void) = {
    (void (*)(void))&__stack_top,
    Reset_Handler,
    NMI_Handler, HardFault_Handler, MemManage_Handler, BusFault_Handler,
    UsageFault_Handler, 0, 0, 0, 0,
    SVC_Handler, DebugMon_Handler, 0,
    PendSV_Handler, SysTick_Handler,
};

void Reset_Handler(void) {
    /* ⚠️ COPIED AND CLEARED HERE, BECAUSE NOBODY ELSE WILL. On a hosted target
     * the loader does this; on a device the image is whatever was written to
     * flash, and `.data`'s initialisers sit in flash while the variables live
     * in RAM. Skipping it produces a program whose globals hold whatever the
     * part powered up with — which on QEMU is zero, so it works there and fails
     * on silicon. */
    unsigned* dst = &__data_start;
    unsigned* src = &__data_load;
    while (dst < &__data_end) *dst++ = *src++;
    for (dst = &__bss_start; dst < &__bss_end; ) *dst++ = 0;

    /* ⚠️⚠️ THE THREAD POINTER, AND WITHOUT IT A C LIBRARY FAULTS BEFORE ITS
     * FIRST OUTPUT.
     *
     * A freestanding image has no thread pointer until something sets one, and
     * picolibc reaches its `stdout` through thread-local storage. Measured with
     * this call absent: a `printf` program built against `mcpplibs/picolibc`
     * linked cleanly, ran, printed NOTHING and hung — the access faulted, the
     * default handler span, and semihosting was never reached. No diagnostic
     * exists for that state; the only evidence is silence.
     *
     * Weak, because a program on the zero-libc tier has no C library to
     * initialise and must not be made to link one. The C library defines these
     * when it is present, and this file's own definitions are used otherwise.
     *
     * ⭐ This is the half of the startup contract a BOARD owns. picolibc's own
     * `picocrt` does the same three things and then decides which host layer to
     * reach — which is a board decision, so this package makes it rather than
     * taking picocrt's. */
    _init_tls(__tls_base);
    _set_tls(__tls_base);
    __libc_init_array();

    board_main();

    /* The entry returning is not an error and not a success: there is no
     * caller and no operating system to report to. A project that wants a
     * status reported calls `board::exit`. */
    for (;;) {}
}
