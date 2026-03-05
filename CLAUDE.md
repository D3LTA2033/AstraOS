                                     
claude --resume fcfa9ec8-edf1-4ea7-92d1-f68894c6e1ab

# AstraOS - Project Instructions

## Overview
AstraOS is a custom operating system with a modular hybrid kernel (AstraCore).
Target architectures: x86 (i686) and x86_64. Development and testing via QEMU.

## Build Commands
- `make` — Build kernel ELF binary
- `make iso` — Build bootable ISO
- `make run` — Build and boot in QEMU
- `make clean` — Remove build artifacts

## Architecture
- `arch/x86/` — x86-specific code (boot assembly, linker script)
- `kernel/core/` — Platform-independent kernel core
- `kernel/drivers/` — Device drivers (each in own subdirectory)
- `kernel/lib/` — Kernel utility library
- `include/` — Global headers (kernel/, drivers/)

## Toolchain
- Cross-compiler: i686-elf-gcc, i686-elf-ld
- Assembler: NASM (elf32 format)
- ISO: grub-mkrescue + xorriso
- Testing: qemu-system-i386

## Coding Standards
- C11, compiled with -Wall -Wextra -Werror -ffreestanding -nostdlib
- No libc dependencies
- Architecture-specific code stays in arch/
- All kernel functions prefixed with k (kstrlen, kmemset, etc.)
- Driver headers in include/drivers/, kernel headers in include/kernel/

## Current Phase
Phase 1: Boot Foundation — complete
Phase 2: Memory + Interrupts — complete
Phase 3: Paging + Heap — complete
Phase 4: Tasking + Scheduler — complete
Phase 5: User Mode + Syscalls — complete
Phase 6: Filesystem + Driver Model — complete
Phase 7: Security Core — complete
