// The board's surface: what a program on this machine can do without knowing
// which machine it is.
//
// ⚠️ SEMIHOSTING RATHER THAN A UART ADDRESS. A UART's address is a fact about
// one part; semihosting is a fact about the debug architecture, and every
// Cortex-M has it — under an emulator and under a probe alike. So this file is
// the same in both environments, which is what lets one package serve both.
//
// ⭐ AND IT IS WHAT MAKES `mcpp test` WORK ON A DEVICE AT ALL. A test binary
// that could print but not report its exit status would leave the verdict to a
// human reading a serial log.
export module mcpplibs.cortex_m_rt;

export namespace board {

// The semihosting call. `bkpt 0xAB` is the M-profile encoding; the host-side
// debug agent — the emulator's or the probe's — reads r0 and r1 and performs
// the operation.
//
// ⚠️ A FUNCTION AND NOT A MACRO, and `register` asms because the operation
// number and the argument must be in r0 and r1 exactly. Nothing else about the
// call is architectural.
inline void semihost(int op, const void* arg) noexcept {
    register int         r0 __asm__("r0") = op;
    register const void* r1 __asm__("r1") = arg;
    __asm__ volatile("bkpt 0xAB" :: "r"(r0), "r"(r1) : "memory");
}

// Write a NUL-terminated string to the host's console. `SYS_WRITE0`.
inline void print(const char* s) noexcept { semihost(0x04, s); }

// ⚠️ `SYS_EXIT_EXTENDED` (0x20) AND NOT `SYS_EXIT` (0x18), AND THE DIFFERENCE
// IS NOT COSMETIC.
//
// On AArch32 `SYS_EXIT` takes the reason code in r1 directly; the `{reason,
// code}` block is the EXTENDED call, which exists because a 32-bit r1 cannot
// carry both a reason and an exit status. Passing the block to 0x18 prints
// correctly and then reports the WRONG status — measured on ARMv7-A, where a
// program exiting 0 reported 1. M-profile has the same two calls and the same
// trap.
[[noreturn]] inline void exit(int code) noexcept {
    struct { unsigned reason; unsigned code; } block{
        0x20026u,                      // ADP_Stopped_ApplicationExit
        static_cast<unsigned>(code) };
    semihost(0x20, &block);
    // Reached only if no debug agent is attached, which is a legitimate state:
    // a board powered up without a probe runs the program and nobody is
    // listening. Spinning is the honest behaviour.
    for (;;) {}
}

}  // namespace board

// ⭐ WHAT LETS A PROGRAM KEEP WRITING `int main()`.
//
// The startup file calls `board_main`, because a C++ `main` compiled for a
// freestanding target is MANGLED — measured: `_Z4mainv` — and a startup file
// written in C cannot name it. Rather than making every program on this board
// spell its entry differently from every program everywhere else, the board
// supplies the bridge.
//
// ⚠️ `extern "C"` AND WEAK. Weak so that a program which genuinely wants the
// unmangled entry can define `board_main` itself and take the whole reset path;
// `extern "C"` so the name the linker sees is the one `start.c` referenced.
extern "C" int main();

extern "C" __attribute__((weak)) int board_main() { return main(); }
