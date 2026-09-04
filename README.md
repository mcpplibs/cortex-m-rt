# cortex-m-rt

Board support for **Cortex-M** — startup, memory layout, a console, and the two
ways of reaching the machine. Depend on it and a bare-metal C++ project builds,
runs and reports an exit status with no board knowledge of its own.

```bash
mcpp new blinky --template mcpplibs/cortex-m-rt
cd blinky
mcpp run                        # under an emulator
mcpp run --features hardware    # on a board, over a debug probe
```

## ⭐⭐ One package, two environments

A board reached through an emulator and the same board reached through a debug
probe differ in the argv of their runners and in **nothing else**: the linker
script, the startup code, the memory map and the exported module are the same
board. So the environment is a *feature*, and the consumer selects it where it
selects everything else.

| | `mcpp run` does | what it installs |
|---|---|---|
| default (`emulator`) | `qemu-system-arm -kernel …` | `xim:qemu-arm`, on the first **run** |
| `--features hardware` | `probe-rs run` — flash, reset, stream, report | `xim:probe-rs`, on the first **run** |

`mcpp build` installs neither. Both tools are declared on the `run` tier, so a CI
job that compiles firmware and never flashes it downloads nothing.

⚠️ Features are additive, so `--features hardware` *adds* the probe and keeps the
emulator — which is right for trying a board once. A project that has moved to
hardware says so on the dependency and stops paying for the emulator at all:

```toml
cortex-m-rt = { version = "0.1.0", default-features = false,
                features = ["hardware"] }
```

## `mcpp run` is the whole of the common case

On a device, *running a program* means writing it, resetting, attaching to its
output and reading its exit status — which is **one** command, not several. So
that is the **default** runner in both environments, and the command a developer
types does not change when the board arrives.

The exceptions have names, and the engine knows none of them:

```bash
mcpp run --runner flash    # write the image and stop
mcpp run --runner serve    # a debug server for an IDE to attach to
mcpp run --list-runners    # what THIS project supplies
```

A top-level `mcpp flash` would be a dead command in every project that is not
firmware; a top-level command surface that varies per project is worse still. So
the variation lives in an option, and a package supplies the vocabulary.

## What the package provides

| | |
|---|---|
| `import mcpplibs.cortex_m_rt` | `board::print`, `board::exit` over semihosting |
| `src/start.c` | the vector table, `.data` copy, `.bss` clear, and weak handlers |
| `cortex-m.ld` | a memory map that boots under the emulator |
| `build.mcpp` | the runners, and the emulator machine for each target row |

Every vector slot is **weak**, which is how a project takes one without editing
this package: defining `SysTick_Handler` anywhere in the program replaces the
default spin. That is also how a scheduler built on
[`openarch`](https://github.com/mcpplibs/openarch) installs its own PendSV.

⚠️ **Semihosting, not a UART address.** A UART's address is a fact about one
part; semihosting is a fact about the debug architecture, and every Cortex-M has
it — under an emulator and under a probe alike. That is what lets one package
serve both, and what makes `mcpp test` work on a device at all: without an exit
status, a test binary could print its verdict and have no way to report it.

## The C library, when you want one

```bash
mcpp run --features libc
```

`picolibc`, as a **source package** compiled with your program's own flags — so
there is no multilib to match and no ABI convention to get wrong. Not selecting
it leaves the tier exactly as it was.

⚠️ **The board sets the thread pointer, and without it a C library faults before
its first output.** picolibc reaches `stdout` through thread-local storage;
`src/start.c` calls `_init_tls`/`_set_tls` and the linker script defines the
five symbols they read. Measured with that missing: a `printf` program linked
cleanly, ran, printed nothing and hung. There is no diagnostic for that state.

## The zero-libc tier

Every `thumb*-none-eabi*` row in mcpp's target table carries an empty C-library
column, so a project targeting one begins with no libc unless it asks. This
package stays there: it references no C library symbol.

A C library arrives through the `libc` feature above.

⚠️ **Floating point on a soft-float row needs compiler builtins.** `-mfpu=none`
makes the compiler lower a `float` multiply onto `__aeabi_fmul`, and at this tier
there is nothing for that call to resolve against. Integer programs are
unaffected; `--features libc` brings both the C library and the builtins.

## Changing the part

`[build] target`. mcpp carries seven M-profile rows and they are not
interchangeable — an object built for `thumbv7em` uses instructions a Cortex-M0
does not have, so picking the row is picking the part. `build.mcpp` maps each row
to the emulator machine that models it.

The memory map in `cortex-m.ld` is QEMU's `mps2-an385`, which is a **default and
not a truth**. A real part has its own; a project supplies one by calling
`mcpp::link_script(...)` in its own `build.mcpp`.

## Licence

Apache-2.0.
