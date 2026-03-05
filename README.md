# AstraOS

A modular hybrid kernel operating system built from scratch, targeting x86 (i686) and x86_64 architectures. AstraOS implements a security-first, production-oriented design philosophy with clean separation between architecture-dependent and architecture-independent subsystems.

## Overview

AstraOS is not a tutorial kernel. It is a long-term systems project structured around a phased development model, where each layer of the operating system is built on top of verified, working infrastructure from the previous phase. The kernel — internally referred to as **AstraCore** — follows a hybrid design pattern: microkernel-inspired modularity for maintainability and security isolation, combined with monolithic-style performance paths where latency matters.

The project enforces strict engineering discipline: all code compiles cleanly under `-Wall -Wextra -Werror -ffreestanding -nostdlib`, with zero tolerance for implicit declarations, unused variables, or unsafe patterns. There are no libc dependencies anywhere in the codebase. Every utility function — from `kstrlen` to `kmemcpy` — is implemented from scratch with full control over behavior and side effects.

## Architecture

```
AstraOS/
├── arch/                       Architecture-dependent subsystems
│   ├── x86/                    32-bit i686 target
│   │   ├── boot/               Multiboot entry, GDT/IDT/TSS flush routines,
│   │   │                       ISR/IRQ stubs, context switch, paging control,
│   │   │                       user-mode transition (IRET-based ring switch)
│   │   ├── include/            Arch-specific headers (port I/O primitives)
│   │   └── linker.ld           Kernel memory layout (load at 1MB, 4K-aligned)
│   └── x86_64/                 64-bit AMD64 target (reserved)
├── kernel/
│   ├── core/                   Platform-independent kernel subsystems
│   │   ├── kernel.c            Primary entry point and initialization sequence
│   │   ├── gdt.c               Global Descriptor Table (flat model, 5 segments + TSS)
│   │   ├── idt.c               Interrupt Descriptor Table (256 vectors, dispatch table)
│   │   ├── pic.c               8259 PIC remapping (IRQ 0-15 → vectors 32-47)
│   │   ├── pmm.c               Physical memory manager (bitmap frame allocator)
│   │   ├── vmm.c               Virtual memory manager (two-level paging, identity map)
│   │   ├── page_fault.c        Page fault handler (ISR 14, CR2 decode, diagnostic panic)
│   │   ├── kheap.c             Kernel heap (first-fit free-list, auto-grow via VMM)
│   │   ├── scheduler.c         Round-robin preemptive scheduler (PIT-driven, 50ms quantum)
│   │   ├── syscall.c           System call dispatch (int 0x80, 5 syscalls)
│   │   └── usermode.c          User-space task creation (ring 0 → ring 3 transition)
│   ├── drivers/
│   │   ├── vga/vga.c           VGA text-mode driver (80×25, scrolling, color)
│   │   ├── pit/pit.c           Programmable Interval Timer (100 Hz tick source)
│   │   └── keyboard/keyboard.c PS/2 keyboard (scancode set 1, US QWERTY)
│   └── lib/
│       └── kstring.c           Freestanding string utilities (kstrlen, kmemset, kmemcpy)
├── include/
│   ├── kernel/                 Kernel subsystem headers
│   └── drivers/                Driver interface headers
├── user/                       User-space programs
│   ├── entry.asm               User entry stub (_start at link base)
│   ├── test_user.c             Ring 3 test program (exercises all syscalls)
│   └── user.ld                 User program linker script (base 0x00400000)
├── config/grub/grub.cfg        GRUB bootloader configuration
├── scripts/run-qemu.sh         QEMU launch helper (supports --debug for GDB attach)
└── Makefile                    Top-level build system
```

### Design Principles

**Strict architecture isolation.** All code that touches CPU-specific registers, instructions, or memory-mapped hardware lives under `arch/`. The kernel core is written in portable C11 and communicates with architecture-dependent code through well-defined interfaces. When x86_64 support is added, nothing outside `arch/x86_64/` should need to change.

**No magic numbers without justification.** The kernel loads at the 1MB boundary because that is the first address above the real-mode IVT, BDA, VGA buffer, and BIOS ROM. Sections are 4KB-aligned because that is the x86 page size, and misalignment would waste TLB entries or cause protection faults when paging is enabled. The PIT runs at 100 Hz because 10ms ticks provide sufficient granularity for a 50ms scheduler quantum without excessive interrupt overhead.

**Security boundaries from day one.** The GDT defines both ring 0 and ring 3 segments from the first phase they are needed. The TSS is installed and maintained so that ring transitions have correct kernel stack pointers. System call arguments are bounds-checked against the user/kernel address space boundary before the kernel dereferences them. The PMM marks physical frame 0 as permanently reserved to catch NULL pointer dereferences at the hardware level.

## Kernel Subsystems

### Memory Management

The memory subsystem is split into three layers:

**Physical Memory Manager (PMM)** — A bitmap-based frame allocator that discovers available RAM by parsing the Multiboot memory map provided by GRUB. The bitmap is placed immediately after the kernel image in physical memory, so its size scales naturally with installed RAM. All frames default to "used" on initialization; only regions explicitly reported as available by the firmware are freed. This is deliberately conservative: if the BIOS fails to report a reserved region, the PMM will not accidentally hand it out.

**Virtual Memory Manager (VMM)** — Manages the x86 two-level page table hierarchy (1024-entry Page Directory → 1024-entry Page Tables → 4KB pages). On initialization, the first 4MB of physical memory is identity-mapped (virtual address equals physical address) to cover the kernel image, VGA buffer, and boot structures. The kernel heap occupies a separate virtual region starting at `0xC0000000`, mapped on demand via the PMM. Individual page mappings can be created or destroyed at runtime with automatic TLB invalidation.

**Kernel Heap** — A first-fit free-list allocator providing `kmalloc`, `kfree`, and `kcalloc`. Each allocation carries an 8-byte header containing the block size and free/used flag. Adjacent free blocks are coalesced on every `kfree` call to reduce fragmentation. When the free list cannot satisfy a request, the heap transparently grows by requesting new pages from the VMM, which in turn allocates physical frames from the PMM. The initial heap is 1MB; the maximum is 256MB.

### Interrupt Infrastructure

**GDT** — Five segment descriptors (null, kernel code, kernel data, user code, user data) plus a Task State Segment. All code/data segments span the full 4GB address space in a flat memory model; segmentation is effectively disabled. The TSS is maintained with the current task's kernel stack pointer, which the CPU loads automatically on ring 3 → ring 0 transitions.

**IDT** — 256 interrupt gates. Vectors 0–31 are CPU exceptions (divide error, page fault, GPF, etc.), each with a dedicated assembly stub that normalizes the stack frame by pushing a dummy error code for exceptions that don't provide one. Vectors 32–47 are hardware IRQs from the remapped 8259 PIC. Vector 128 (`int 0x80`) is the system call entry point, configured with DPL=3 so user-mode code can invoke it.

**PIC** — The dual 8259 Programmable Interrupt Controllers are remapped during initialization so that IRQ 0–7 map to vectors 32–39 and IRQ 8–15 map to vectors 40–47. This is necessary because the BIOS default mapping (IRQ 0–7 → vectors 8–15) collides with CPU exception vectors. After remapping, all IRQ lines are masked; individual drivers unmask their IRQ when they register a handler.

### Scheduler

The scheduler implements preemptive round-robin multitasking. Tasks are stored in a circular linked list. The PIT timer fires at 100 Hz, and after every 5 ticks (50ms quantum), the scheduler selects the next ready task and performs a context switch.

Context switching is implemented in hand-written assembly. The `context_switch` routine saves the current task's callee-saved registers (EBP, EBX, ESI, EDI, EFLAGS) and stack pointer, then loads the new task's stack pointer and restores its registers. Caller-saved registers (EAX, ECX, EDX) are handled by the C calling convention and do not need explicit save/restore.

New tasks are bootstrapped through a trampoline mechanism. `task_create` prepares a fake stack frame that looks like `context_switch` just saved the registers, with the return address pointing to `task_trampoline`. When the scheduler first switches to the new task, `context_switch` pops the fake frame, `ret` jumps to the trampoline, and the trampoline calls the actual entry function. If the entry function returns, the trampoline invokes `task_exit` to clean up.

Dead tasks are reaped lazily during scheduling — the reaper runs at the beginning of each `schedule()` call and removes any task in the DEAD state from the circular list, freeing its kernel stack and task control block.

### System Calls

User-space programs invoke system calls through `int 0x80`. The syscall number is passed in EAX, with up to five arguments in EBX, ECX, EDX, ESI, and EDI. The return value is placed in EAX.

| Number | Name       | Arguments                          | Description                          |
|--------|------------|------------------------------------|--------------------------------------|
| 0      | `SYS_EXIT`  | `ebx` = exit code                 | Terminate the calling task           |
| 1      | `SYS_WRITE` | `ebx` = fd, `ecx` = buf, `edx` = len | Write to file descriptor (fd=1: VGA) |
| 2      | `SYS_GETPID`| (none)                             | Return the calling task's ID         |
| 3      | `SYS_YIELD` | (none)                             | Voluntarily yield the time slice     |
| 4      | `SYS_SLEEP` | `ebx` = ticks                     | Sleep for N timer ticks (10ms each)  |

Argument validation is performed before any kernel operation. Buffer pointers from user space are checked against the kernel/user boundary (`0xC0000000`) to prevent user programs from tricking the kernel into reading or writing kernel memory.

### User Mode

The transition from ring 0 to ring 3 is performed by constructing a fake IRET frame on the kernel stack. The frame contains the user code segment selector (`0x1B` = GDT entry 3 with RPL 3), user data segment selector (`0x23` = GDT entry 4 with RPL 3), the user entry point, user stack pointer, and EFLAGS with the interrupt flag set. When `IRET` executes, the CPU loads these values and drops to ring 3.

User programs are compiled as flat binaries linked at `0x00400000`. The kernel maps physical frames at this virtual address with the user-accessible flag set, copies the program binary into the mapped pages, and similarly maps a 16KB user stack below `0x00800000`. A kernel-side wrapper task handles the ring transition and TSS setup.

## Toolchain

AstraOS is built with a freestanding cross-compiler toolchain. No host compiler or system libraries are used at any point in the build process.

| Tool             | Version | Purpose                                      |
|------------------|---------|----------------------------------------------|
| `i686-elf-gcc`   | 13.2.0  | Cross-compiler targeting bare-metal i686-elf  |
| `i686-elf-ld`    | 2.41    | Linker with custom linker script support      |
| `i686-elf-objcopy`| 2.41   | Binary extraction from ELF for user programs  |
| `nasm`           | latest  | Assembler for boot stubs and low-level routines|
| `grub-mkrescue`  | 2.12    | ISO 9660 image generation with GRUB stage 2   |
| `xorriso`        | latest  | Backend for grub-mkrescue ISO creation         |
| `qemu-system-i386`| 8.2    | Hardware emulation for development testing     |

### Building the Cross-Compiler

The cross-compiler must be built from source. A build script is provided at `scripts/` or can be done manually:

```bash
export PREFIX="/opt/cross"
export TARGET="i686-elf"
export PATH="$PREFIX/bin:$PATH"

# Build binutils 2.41
tar xf binutils-2.41.tar.xz && cd binutils-2.41
./configure --target=$TARGET --prefix=$PREFIX --with-sysroot --disable-nls --disable-werror
make -j$(nproc) && make install

# Build GCC 13.2.0 (C only, no hosted headers)
tar xf gcc-13.2.0.tar.xz && cd gcc-13.2.0
./configure --target=$TARGET --prefix=$PREFIX --disable-nls --enable-languages=c --without-headers
make -j$(nproc) all-gcc all-target-libgcc
make install-gcc install-target-libgcc
```

## Building

```bash
export PATH="/opt/cross/bin:$PATH"

make            # Build kernel ELF binary
make iso        # Build bootable GRUB ISO
make run        # Build + boot in QEMU (graphical, serial on stdio)
make clean      # Remove all build artifacts
```

The build system compiles kernel C sources and NASM assembly, builds the user-space test program as a flat binary, embeds it into the kernel via `ld -b binary`, and links everything against the custom linker script. The resulting ELF is then packaged into a bootable ISO with GRUB.

## Running

```bash
# Standard boot (graphical window + serial on terminal)
make run

# Headless boot (text only)
make run-nographic

# Debug boot (QEMU paused, GDB server on :1234)
./scripts/run-qemu.sh --debug

# In another terminal:
gdb build/astra.kernel -ex "target remote :1234"
```

QEMU is configured with 128MB RAM, CD-ROM boot from the ISO, and serial output on stdio. The `-no-reboot` flag prevents infinite reboot loops on triple faults during development.

## Development Roadmap

| Phase | Name                    | Status    | Description |
|-------|-------------------------|-----------|-------------|
| 1     | Boot Foundation         | Complete  | Multiboot, GDT bootstrap, VGA console, GRUB ISO boot |
| 2     | Memory + Interrupts     | Complete  | GDT/IDT/TSS, PIC remapping, PIT timer, keyboard, PMM |
| 3     | Paging + Heap           | Complete  | Two-level paging, identity map, kernel heap allocator |
| 4     | Tasking + Scheduler     | Complete  | Context switching, round-robin preemption, task lifecycle |
| 5     | User Mode + Syscalls    | Complete  | Ring 3 execution, int 0x80 dispatch, user program loading |
| 6     | Filesystem + Driver Model| Planned  | VFS layer, initramfs/ramfs, modular driver registration |
| 7     | Security Core           | Planned   | Capability-based access control, address space isolation |

## Memory Layout

### Physical Memory
```
0x00000000 - 0x000FFFFF    Low memory (IVT, BDA, VGA buffer, BIOS ROM)
0x00100000 - 0x001XXXXX    Kernel image (.multiboot, .text, .rodata, .data, .bss)
0x001XXXXX - 0x001XXXXX    PMM bitmap (scales with installed RAM)
0x00200000+                 Free physical frames managed by PMM
```

### Virtual Memory
```
0x00000000 - 0x003FFFFF    Identity-mapped (kernel + hardware, PAGE_KERNEL)
0x00400000 - 0x004XXXXX    User program code (PAGE_USER)
0x007FC000 - 0x007FFFFF    User stack (PAGE_USER, grows downward)
0xC0000000 - 0xCFFFFFFF    Kernel heap (PAGE_KERNEL, grows via VMM)
```

## Coding Conventions

- **C11 standard**, compiled with maximum warnings as errors
- All kernel-space utility functions are prefixed with `k` (`kstrlen`, `kmemset`, `kmalloc`)
- Architecture-specific code is strictly isolated under `arch/`
- Driver interfaces are defined in `include/drivers/`, implementations in `kernel/drivers/`
- No dynamic memory allocation during interrupt handling
- All interrupt handlers send EOI before returning
- User-space buffer pointers are validated before kernel dereference

## License

This project is currently under active development. License terms will be specified in a future release.
