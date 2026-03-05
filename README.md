<p align="center">
  <img src="https://img.shields.io/badge/platform-x86-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/language-C11%20%7C%20NASM-orange?style=flat-square" alt="Language">
  <img src="https://img.shields.io/badge/kernel-AstraCore-blueviolet?style=flat-square" alt="Kernel">
  <img src="https://img.shields.io/badge/lines%20of%20code-12.7k-green?style=flat-square" alt="Lines">
  <img src="https://img.shields.io/badge/license-MIT-lightgrey?style=flat-square" alt="License">
</p>

<h1 align="center">AstraOS</h1>

<p align="center">
  <b>A from-scratch operating system with a Hyprland-inspired graphical desktop,<br>
  built-in programming language, and glassmorphism window compositor.</b>
</p>

<p align="center">
  <i>12,700+ lines of handwritten C and assembly. Zero dependencies. No libc. Boots on anything with a BIOS.</i>
</p>

---

## What is AstraOS?

AstraOS is a complete operating system written from scratch targeting x86 (i686) hardware. It boots via GRUB, initializes its own memory manager, scheduler, filesystem, and graphical desktop environment — all running in a single ~300KB kernel binary.

The kernel — internally called **AstraCore** — follows a hybrid design: microkernel-inspired modularity for maintainability combined with monolithic-style performance paths where latency matters. Every line of code compiles cleanly under `-Wall -Wextra -Werror -ffreestanding -nostdlib`. There are no libc dependencies anywhere. Every utility function — from `kstrlen` to `kmemcpy` to the entire GUI compositor — is implemented from scratch.

### Key Features

- **Glassmorphism Desktop** — Hyprland-inspired compositing window manager with alpha-blended glass surfaces, rounded corners, gradient wallpapers, multi-layer soft shadows, and macOS-style window control dots
- **12 Built-in Themes** — Catppuccin Mocha, Nord, Dracula, Gruvbox, Tokyo Night, Rose Pine, Solarized Dark, One Dark, Kanagawa, Everforest, Cyberpunk, Midnight Blue — all with per-theme transparency levels and syntax color palettes
- **7 Procedural Wallpapers** — Gradient, Mesh, Waves, Geometric, Particles, Aurora, and Solid — generated in real-time using integer math only
- **VS Code-Like Code Editor** — Sidebar file explorer, multi-tab editing, minimap, line numbers, syntax highlighting, auto-indent, bracket auto-close, Run button, output panel, and a VS Code-style status bar
- **Astracor Programming Language** — Complete interpreted language with dynamic typing, functions, closures, control flow, arrays, 24 built-in functions, 4 importable standard library modules, and a recursive descent parser
- **Preemptive Multitasking** — Round-robin scheduler with 50ms quantum, context switching in hand-written assembly, ring 0/ring 3 isolation
- **Virtual Filesystem** — VFS layer with ramfs, devfs, procfs, and initramfs support
- **First-Boot Installer** — Arch Linux-inspired graphical setup wizard with hostname, username, and theme configuration

---

## Screenshots

The desktop runs at 1024x768x32bpp via VESA framebuffer with full double buffering:

```
+-----------------------------------------------------------------------+
|                    Gradient Wallpaper (deep purple-blue)               |
|  +--Code Editor------------------+  +--File Manager--+                |
|  | * * * |    Code Editor        |  | * * * | Files  |                |
|  |EXPLORER|  [main.ac]           |  |New File| Open  |                |
|  | > dev  | 1  # Welcome...     |  | /      |       |                |
|  |   etc  | 2  fn greet(name) { |  |  dev/  |       |                |
|  |   proc | 3    print("Hello") |  |  etc/  |       |                |
|  |   bin  | 4  }                |  |  proc/ |       |                |
|  |        | 5  greet("World")   |  |  bin/  |       |                |
|  |        |   [Run F5] OUTPUT   |  +--------+-------+                |
|  |        |  main.ac|Astracor|  |  +--Settings-------+               |
|  +--------+---------------------+  | Themes|Wallpaper|               |
|  +--Terminal---------------------+  | [Catppuccin]    |               |
|  | * * * |  Astracor Terminal    |  | [Nord]          |               |
|  | >> print("hello")             |  | [Dracula]       |               |
|  | hello                         |  +-----------------+               |
|  +-------------------------------+                                    |
|  [AstraOS] [Code Editor] [Terminal] [File Mgr]    12:34:56           |
+-----------------------------------------------------------------------+
```

All windows have transparent glass backgrounds that show the wallpaper gradient through them. The taskbar uses pill-shaped buttons with active indicator dots.

---

## Architecture

```
AstraOS/
├── arch/x86/                          Architecture-dependent (i686)
│   ├── boot/
│   │   ├── boot.asm                   Multiboot entry, stack setup, kernel_main call
│   │   ├── gdt_flush.asm             GDT/TSS reload
│   │   ├── isr.asm                   48 ISR/IRQ stubs + common handler
│   │   ├── paging.asm                CR3 load, TLB invalidation
│   │   ├── context_switch.asm        Task context save/restore
│   │   └── usermode.asm              Ring 0 → Ring 3 IRET transition
│   ├── include/io.h                   Port I/O primitives (inb/outb)
│   └── linker.ld                      Kernel memory layout (load at 1MB)
│
├── kernel/
│   ├── core/                          Platform-independent kernel core
│   │   ├── kernel.c                   Entry point, init sequence, GUI main loop
│   │   ├── gdt.c                      Global Descriptor Table (5 segments + TSS)
│   │   ├── idt.c                      Interrupt Descriptor Table (256 vectors)
│   │   ├── pic.c                      8259 PIC remapping (IRQ→vectors 32-47)
│   │   ├── pmm.c                      Physical memory manager (bitmap allocator)
│   │   ├── vmm.c                      Virtual memory manager (two-level paging)
│   │   ├── page_fault.c              Page fault handler (CR2 decode, panic)
│   │   ├── kheap.c                    Kernel heap (first-fit, auto-grow via VMM)
│   │   ├── scheduler.c               Preemptive round-robin scheduler
│   │   ├── syscall.c                  System call dispatch (int 0x80)
│   │   ├── usermode.c                User-space task creation
│   │   ├── vfs.c                      Virtual Filesystem switch
│   │   ├── devfs.c                    Device filesystem (/dev)
│   │   ├── ramfs.c                    RAM filesystem
│   │   ├── initramfs.c               Initial ramdisk loader
│   │   ├── capability.c              Capability-based security
│   │   ├── pipe.c                     IPC pipes
│   │   ├── signal.c                   Signal handling
│   │   ├── gui.c                      Compositing window manager
│   │   ├── theme.c                    Theme engine (12 themes, 7 wallpapers)
│   │   ├── apps.c                     Editor, Terminal, File Manager, Settings
│   │   ├── astracor.c                 Astracor language interpreter
│   │   └── installer.c               First-boot setup wizard
│   │
│   ├── drivers/
│   │   ├── framebuffer/               VESA framebuffer (alpha, gradients, rounded rects)
│   │   │   ├── framebuffer.c          Graphics primitives + double buffering
│   │   │   └── font8x16.c            128-glyph bitmap font
│   │   ├── vga/vga.c                  VGA text mode (80x25, fallback)
│   │   ├── keyboard/keyboard.c       PS/2 keyboard (scancode set 1, US QWERTY)
│   │   ├── mouse/mouse.c             PS/2 mouse driver
│   │   ├── pit/pit.c                 Programmable Interval Timer (100 Hz)
│   │   ├── serial/serial.c           Serial port (COM1, 115200 baud)
│   │   └── rtc/rtc.c                 Real-time clock
│   │
│   └── lib/
│       └── kstring.c                  Freestanding utilities (kstrlen, kmemcpy, etc.)
│
├── include/
│   ├── kernel/                        Kernel subsystem headers (34 files)
│   │   ├── gui.h                      Window manager API
│   │   ├── theme.h                    Theme engine API
│   │   ├── astracor.h                 Language interpreter API
│   │   ├── apps.h                     Application APIs
│   │   └── ...                        (types, vfs, scheduler, syscall, etc.)
│   └── drivers/
│       ├── framebuffer.h              Graphics API + color palette
│       └── ...                        (keyboard, mouse, rtc, etc.)
│
├── user/                              User-space programs (ring 3)
│   ├── test_user.c                    Interactive shell (syscall exerciser)
│   ├── hello.c                        Hello world
│   ├── fib.c                          Fibonacci demo
│   ├── entry.asm                      User entry stub
│   └── user.ld                        User linker script (base 0x00400000)
│
├── config/grub/grub.cfg               GRUB config (graphical + text fallback)
└── Makefile                           Top-level build system
```

### Design Principles

**Strict architecture isolation.** All code that touches CPU-specific registers, instructions, or memory-mapped hardware lives under `arch/`. The kernel core is portable C11 that communicates through well-defined interfaces. When x86_64 support is added, nothing outside `arch/x86_64/` should need to change.

**Zero external dependencies.** No libc, no POSIX, no borrowed code. Every function — from string operations to the language parser to the alpha-blending compositor — is written from first principles. The entire OS compiles with a freestanding cross-compiler and runs on bare metal.

**Security boundaries from day one.** The GDT defines ring 0 and ring 3 segments from the moment they are needed. The TSS maintains correct kernel stack pointers for ring transitions. Syscall arguments are bounds-checked against the user/kernel boundary before dereferencing. The PMM marks frame 0 as reserved to catch NULL dereferences at the hardware level. A capability-based security model governs resource access.

**Integer-only graphics.** The entire GUI — including alpha blending, gradient interpolation, rounded corner hit-testing, and procedural wallpaper generation — uses only integer arithmetic. No floating-point unit is touched. This means the compositor works identically on any x86 CPU from a Pentium to a modern Core i9.

---

## The Desktop Environment

### Window Manager

The compositing window manager renders all windows back-to-front with full alpha blending. Every frame is drawn to a back buffer and swapped to the framebuffer atomically to prevent tearing.

**Glass Effect:** Window bodies are drawn with semi-transparent backgrounds (alpha varies by theme, typically 170-215/255). The desktop wallpaper gradient bleeds through, creating a modern glassmorphism effect. Focused windows get slightly higher opacity and a glowing accent-colored border.

**Window Chrome:**
- Rounded corners (10px radius) on all windows
- Multi-layer soft shadows (4 alpha-blended rectangles at increasing offsets)
- macOS-style control dots: red (close), yellow (minimize), green (maximize)
- Centered title text in the title bar
- Click-and-drag window movement via title bar

**Taskbar:**
- Glass-effect semi-transparent background with accent-colored highlight line
- Pill-shaped "AstraOS" logo button
- Pill-shaped buttons for each open window
- Active window indicator dot
- Real-time clock (HH:MM:SS from RTC)

### Theme Engine

The theme engine provides centralized color management. All GUI components — windows, taskbar, editor, terminal, file manager, settings — read colors from the active theme rather than hardcoding values.

Each theme defines 28 color properties:

| Category | Properties |
|----------|-----------|
| **Base** | `bg`, `bg_dark`, `surface`, `overlay` |
| **Text** | `text`, `text_dim`, `text_bright` |
| **Accent** | `accent`, `accent2`, `highlight` |
| **Syntax** | `syn_keyword`, `syn_string`, `syn_number`, `syn_comment`, `syn_function`, `syn_operator`, `syn_bracket` |
| **Chrome** | `border`, `titlebar`, `taskbar`, `red`, `green`, `yellow` |
| **Wallpaper** | `wall_top`, `wall_bottom`, `wall_glow` |
| **Transparency** | `window_alpha`, `titlebar_alpha`, `taskbar_alpha` |

**Available Themes:**

| # | Theme | Style |
|---|-------|-------|
| 0 | Catppuccin Mocha | Warm purple-blue, pastel accents |
| 1 | Nord | Arctic cool blue-gray |
| 2 | Dracula | Dark purple with neon cyan/pink |
| 3 | Gruvbox Dark | Warm retro brown-orange |
| 4 | Tokyo Night | Deep navy with soft purple |
| 5 | Rose Pine | Dark mauve with rose gold |
| 6 | Solarized Dark | Teal-based scientific palette |
| 7 | One Dark | Atom's signature dark theme |
| 8 | Kanagawa | Japanese wave-inspired blues |
| 9 | Everforest | Forest green organic tones |
| 10 | Cyberpunk | Neon magenta on deep purple |
| 11 | Midnight Blue | Deep ocean blue with bright accents |

### Procedural Wallpapers

All wallpapers are generated in real-time using integer-only math:

- **Gradient** — Vertical gradient between theme's `wall_top` and `wall_bottom` with a centered alpha-blended accent glow
- **Mesh** — Multiple overlapping alpha-blended color blobs creating an organic gradient mesh
- **Waves** — Animated horizontal wave bands using triangle-wave approximation of sine
- **Geometric** — Random rectangles, diamonds, and circles in theme accent colors
- **Particles** — Starfield with hundreds of white dots and colored nebula blobs
- **Aurora** — Simulated aurora borealis with vertical curtain bands that shift with time
- **Solid** — Simple solid color fill

---

## Built-in Applications

### Code Editor

A VS Code-inspired editor running entirely in kernel space:

- **Multi-tab editing** — Up to 8 simultaneous files with tab bar navigation
- **Sidebar explorer** — Toggleable file tree showing the VFS hierarchy; click to open files in new tabs
- **Minimap** — 60px zoomed-out code overview with syntax-colored pixels and viewport indicator; click to scroll
- **Line numbers** — Right-aligned gutter with current line highlight
- **Syntax highlighting** — Keywords (purple), strings (green), numbers (orange), comments (gray), builtins (blue), brackets (yellow), operators (accent), function calls (yellow)
- **Auto-indent** — Preserves leading whitespace; adds 4 spaces after `{`
- **Bracket auto-close** — `{` inserts `}`, `(` inserts `)`, `[` inserts `]`
- **Run button (F5)** — Executes the current file through the Astracor interpreter; output shown in the bottom panel
- **Status bar** — Filename, language indicator ("Astracor"), cursor position (Ln/Col)

### Astracor Terminal

Interactive REPL for the Astracor language with transparent glass background:

- Line-by-line execution with persistent interpreter state
- Scrollable output buffer (256 lines)
- Command prompt with cursor

### File Manager

VFS browser with directory navigation:

- Folder/file icons with theme-colored directory names
- Toolbar with "New File" and "Open" buttons
- Path breadcrumb bar
- Double-click to navigate directories or open `.ac` files in the editor
- ".." parent directory navigation

### Settings

Interactive theme and wallpaper chooser:

- **Themes tab** — Scrollable list of all 12 themes with color swatch previews and syntax highlighting samples; click to apply instantly
- **Wallpaper tab** — List of 7 wallpaper styles with active indicator
- **About tab** — System information
- Keyboard shortcuts: `[`/`]` to cycle themes, Tab to switch settings tabs

---

## The Astracor Language

Astracor is AstraOS's built-in programming language — a dynamically typed, interpreted language with C-like syntax and modern conveniences. The implementation is a complete recursive descent parser + tree-walking interpreter written in ~1,400 lines of C.

### Language Features

```javascript
# Variables and types
let name = "AstraOS"
let version = 2
let pi_approx = 3141       # Fixed-point: 3.141 * 1000
let items = [1, 2, 3, 4, 5]
let flag = true

# Functions
fn fibonacci(n) {
    if n < 2 { return n }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

# Control flow
for i in range(10) {
    if i % 2 == 0 {
        print(str(i) + " is even")
    } else {
        print(str(i) + " is odd")
    }
}

# String operations
let greeting = upper("hello") + " " + name
print(substr(greeting, 0, 5))         # "HELLO"
print(contains(greeting, "Astra"))     # true

# Module imports
import math
print(math.factorial(10))              # 3628800

import array
let doubled = array.map(items, fn(x) { return x * 2 })
```

### Type System

| Type | Examples | Operations |
|------|----------|------------|
| `int` | `42`, `-7`, `0` | Arithmetic, comparison, bitwise |
| `float` | Fixed-point (val * 1000) | Arithmetic, comparison |
| `string` | `"hello"`, `""` | Concatenation (`+`), indexing, 12 string builtins |
| `bool` | `true`, `false` | Logical AND/OR/NOT |
| `array` | `[1, "two", true]` | Indexing, push/pop, 4 array builtins |
| `null` | `null` | Equality check |
| `fn` | `fn(x) { return x }` | Call, pass as argument |

### Built-in Functions (24)

| Category | Functions |
|----------|-----------|
| **Core** | `print`, `len`, `str`, `int`, `type`, `range`, `abs` |
| **String** | `substr`, `indexof`, `split`, `join`, `upper`, `lower`, `trim`, `replace`, `startswith`, `endswith`, `char_at`, `contains` |
| **Math** | `min`, `max`, `clamp`, `pow` |
| **Array** | `push`, `pop`, `get`, `set` |

### Standard Library Modules

Importable via the `import` keyword. Modules are compiled into the kernel as constant string arrays and executed in the global scope:

| Module | Functions |
|--------|-----------|
| `math` | `PI`, `E`, `square`, `cube`, `factorial`, `gcd`, `lcm`, `is_even`, `is_odd`, `sum_range` |
| `string` | `repeat`, `reverse`, `is_empty`, `pad_left`, `pad_right` |
| `array` | `map`, `filter`, `reduce`, `foreach`, `find`, `sum` |
| `io` | `println`, `debug`, `assert`, `assert_eq` |

---

## Kernel Subsystems

### Memory Management

Three-layer memory hierarchy:

**Physical Memory Manager (PMM)** — Bitmap-based frame allocator that discovers available RAM by parsing the Multiboot memory map. The bitmap is placed after the kernel image, so its size scales with installed RAM. All frames default to "used"; only regions explicitly reported as available by the firmware are freed.

**Virtual Memory Manager (VMM)** — Manages x86 two-level page tables (Page Directory -> Page Tables -> 4KB pages). The first 4MB is identity-mapped for kernel and hardware. The kernel heap starts at `0xC0000000` and maps pages on demand. The VESA framebuffer is identity-mapped at its physical address.

**Kernel Heap** — First-fit free-list allocator with 8-byte block headers. Adjacent free blocks are coalesced on `kfree`. Auto-grows by requesting pages from VMM. Initial size: 1MB, maximum: 256MB.

### Interrupt Infrastructure

- **GDT** — 5 segment descriptors (null, kernel code/data, user code/data) + TSS
- **IDT** — 256 vectors: CPU exceptions (0-31), hardware IRQs (32-47), syscall (128)
- **PIC** — Dual 8259 remapped to avoid CPU exception vector collision

### Scheduler

Preemptive round-robin with 50ms quantum. Context switch saves/restores callee-saved registers (EBP, EBX, ESI, EDI, EFLAGS) and stack pointer in hand-written assembly. New tasks use a trampoline mechanism. Dead tasks are reaped lazily.

### System Calls

User programs invoke syscalls via `int 0x80` (number in EAX, args in EBX-EDI):

| # | Name | Description |
|---|------|-------------|
| 0 | `SYS_EXIT` | Terminate task |
| 1 | `SYS_WRITE` | Write to fd (fd=1: VGA) |
| 2 | `SYS_GETPID` | Get task ID |
| 3 | `SYS_YIELD` | Voluntary yield |
| 4 | `SYS_SLEEP` | Sleep for N ticks |

### Filesystem

**VFS** — Virtual filesystem switch with path resolution, directory traversal, and node creation. Supports mounting of different filesystem backends.

**ramfs** — In-memory filesystem for `/etc` (hostname, version), `/bin` (embedded user programs), and user-created files.

**devfs** — Device filesystem mounting at `/dev` with character device registration (console, serial, RTC).

**procfs** — Process information filesystem at `/proc` with dynamic read handlers for meminfo, uptime, tasks, and cpuinfo.

**initramfs** — Loader for GRUB module-provided initial ramdisk.

### Graphics Pipeline

**Framebuffer Driver** — VESA linear framebuffer initialized from Multiboot info. Supports:
- 32-bit ARGB pixels with full alpha blending
- Rounded rectangle primitives (filled and outline)
- Horizontal and vertical gradient fills
- Circle fill (midpoint algorithm)
- Alpha-blended text rendering
- Double buffering with atomic swap
- Optimized `rep stosl` memory fills

**Compositing** — Every GUI frame: clear to wallpaper, draw windows back-to-front with alpha blending, draw taskbar with glass effect, draw cursor with drop shadow, swap buffer.

---

## Framebuffer Graphics API

The framebuffer driver provides hardware-accelerated (integer-only) 2D graphics:

```c
// Basic primitives
void fb_putpixel(int x, int y, color_t color);
void fb_fill_rect(int x, int y, int w, int h, color_t color);
void fb_rect(int x, int y, int w, int h, color_t color);

// Alpha blending
void fb_blend_pixel(int x, int y, color_t color);
void fb_fill_rect_alpha(int x, int y, int w, int h, color_t color);

// Rounded rectangles
void fb_fill_rounded_rect(int x, int y, int w, int h, int radius, color_t color);
void fb_rounded_rect(int x, int y, int w, int h, int radius, color_t color);

// Gradients
void fb_gradient_v(int x, int y, int w, int h, color_t top, color_t bottom);
void fb_gradient_h(int x, int y, int w, int h, color_t left, color_t right);

// Shapes
void fb_fill_circle(int cx, int cy, int r, color_t color);

// Text (8x16 bitmap font, 128 glyphs)
void fb_puts(int x, int y, const char *s, color_t fg, color_t bg);
void fb_puts_transparent(int x, int y, const char *s, color_t fg);
void fb_puts_alpha(int x, int y, const char *s, color_t fg, color_t bg);

// Color macros
#define RGB(r,g,b)      ((0xFF000000) | ((r)<<16) | ((g)<<8) | (b))
#define RGBA(r,g,b,a)   (((a)<<24) | ((r)<<16) | ((g)<<8) | (b))
#define COL_A(c)         (((c) >> 24) & 0xFF)
#define COL_R(c)         (((c) >> 16) & 0xFF)
// ...
```

---

## Memory Layout

### Physical Memory
```
0x00000000 - 0x000FFFFF    Low memory (IVT, BDA, VGA buffer, BIOS ROM)
0x00100000 - 0x001XXXXX    Kernel image (.multiboot, .text, .rodata, .data, .bss)
0x001XXXXX - 0x001XXXXX    PMM bitmap (scales with installed RAM)
0x00200000+                 Free physical frames managed by PMM
0xFD000000+                 VESA linear framebuffer (identity-mapped)
```

### Virtual Memory
```
0x00000000 - 0x003FFFFF    Identity-mapped (kernel + hardware)
0x00400000 - 0x004XXXXX    User program code (ring 3)
0x007FC000 - 0x007FFFFF    User stack (ring 3, grows downward)
0xC0000000 - 0xCFFFFFFF    Kernel heap (grows via VMM)
```

---

## Installation Guide

AstraOS requires a freestanding cross-compiler toolchain, an assembler, ISO generation tools, and an emulator for testing. Below are complete setup instructions for every major platform.

### Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| `i686-elf-gcc` | 13.2.0+ | Cross-compiler (C11, freestanding, no libc) |
| `i686-elf-ld` | 2.41+ | Linker with custom linker script support |
| `i686-elf-objcopy` | 2.41+ | Binary extraction from ELF for user programs |
| `nasm` | 2.15+ | Assembler for boot stubs (elf32 format) |
| `grub-mkrescue` | 2.06+ | Bootable ISO 9660 image generation |
| `xorriso` | 1.5+ | Backend for grub-mkrescue ISO creation |
| `qemu-system-i386` | 7.0+ | Hardware emulation for development and testing |
| `make` | 4.0+ | Build system |
| `git` | 2.0+ | Source control |

---

### Ubuntu / Debian / Pop!_OS / Linux Mint

```bash
# 1. Install system dependencies
sudo apt update
sudo apt install -y build-essential bison flex libgmp-dev libmpc-dev \
    libmpfr-dev texinfo nasm xorriso grub-pc-bin grub-common mtools \
    qemu-system-x86 git curl

# 2. Build the cross-compiler
export PREFIX="/opt/cross"
export TARGET="i686-elf"
export PATH="$PREFIX/bin:$PATH"
sudo mkdir -p $PREFIX

# Download sources
mkdir -p /tmp/cross-build && cd /tmp/cross-build
curl -LO https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz
curl -LO https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz

# Build binutils
tar xf binutils-2.41.tar.xz
mkdir build-binutils && cd build-binutils
../binutils-2.41/configure --target=$TARGET --prefix=$PREFIX \
    --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
sudo make install
cd ..

# Build GCC
tar xf gcc-13.2.0.tar.xz
mkdir build-gcc && cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix=$PREFIX \
    --disable-nls --enable-languages=c --without-headers
make -j$(nproc) all-gcc all-target-libgcc
sudo make install-gcc install-target-libgcc
cd /tmp && rm -rf cross-build

# 3. Add to PATH (add to ~/.bashrc for persistence)
echo 'export PATH="/opt/cross/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# 4. Clone and build AstraOS
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
make iso

# 5. Run
make run
```

### Fedora / RHEL / CentOS / Rocky Linux / AlmaLinux

```bash
# 1. Install system dependencies
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y gmp-devel mpfr-devel libmpc-devel texinfo \
    nasm xorriso grub2-tools-extra mtools qemu-system-x86 git curl

# 2. Build the cross-compiler (same as Ubuntu)
export PREFIX="/opt/cross"
export TARGET="i686-elf"
export PATH="$PREFIX/bin:$PATH"
sudo mkdir -p $PREFIX

mkdir -p /tmp/cross-build && cd /tmp/cross-build
curl -LO https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz
curl -LO https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz

tar xf binutils-2.41.tar.xz
mkdir build-binutils && cd build-binutils
../binutils-2.41/configure --target=$TARGET --prefix=$PREFIX \
    --with-sysroot --disable-nls --disable-werror
make -j$(nproc) && sudo make install && cd ..

tar xf gcc-13.2.0.tar.xz
mkdir build-gcc && cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix=$PREFIX \
    --disable-nls --enable-languages=c --without-headers
make -j$(nproc) all-gcc all-target-libgcc
sudo make install-gcc install-target-libgcc
cd /tmp && rm -rf cross-build

# 3. Add to PATH
echo 'export PATH="/opt/cross/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# 4. Clone, build, run
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
make run
```

### Arch Linux / Manjaro / EndeavourOS

```bash
# 1. Install dependencies (Arch has cross-compiler packages in AUR)
sudo pacman -S --needed base-devel nasm xorriso grub mtools \
    qemu-system-x86 git curl gmp libmpc mpfr

# Option A: Use AUR packages (easier)
# If you have an AUR helper like yay or paru:
yay -S i686-elf-binutils i686-elf-gcc

# Option B: Build from source (same as Ubuntu step 2)
export PREFIX="/opt/cross"
export TARGET="i686-elf"
export PATH="$PREFIX/bin:$PATH"
sudo mkdir -p $PREFIX

mkdir -p /tmp/cross-build && cd /tmp/cross-build
curl -LO https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz
curl -LO https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz

tar xf binutils-2.41.tar.xz
mkdir build-binutils && cd build-binutils
../binutils-2.41/configure --target=$TARGET --prefix=$PREFIX \
    --with-sysroot --disable-nls --disable-werror
make -j$(nproc) && sudo make install && cd ..

tar xf gcc-13.2.0.tar.xz
mkdir build-gcc && cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix=$PREFIX \
    --disable-nls --enable-languages=c --without-headers
make -j$(nproc) all-gcc all-target-libgcc
sudo make install-gcc install-target-libgcc
cd /tmp && rm -rf cross-build

# 2. Add to PATH
echo 'export PATH="/opt/cross/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# 3. Clone, build, run
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
make run
```

### openSUSE (Tumbleweed / Leap)

```bash
# 1. Install dependencies
sudo zypper install -y -t pattern devel_C_C++
sudo zypper install -y nasm xorriso grub2 mtools qemu-x86 git curl \
    gmp-devel mpfr-devel mpc-devel texinfo

# 2. Build cross-compiler (same steps as Ubuntu step 2)
# ... (see Ubuntu section for the full binutils + GCC build)

# 3. Clone, build, run
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
export PATH="/opt/cross/bin:$PATH"
make run
```

### Void Linux

```bash
# 1. Install dependencies
sudo xbps-install -Su
sudo xbps-install -y base-devel nasm xorriso grub mtools \
    qemu git curl gmp-devel mpfr-devel libmpc-devel texinfo

# 2. Build cross-compiler (same steps as Ubuntu step 2)
# ... (see Ubuntu section for the full binutils + GCC build)

# 3. Clone, build, run
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
export PATH="/opt/cross/bin:$PATH"
make run
```

### Gentoo

```bash
# 1. Install dependencies
sudo emerge --ask sys-devel/crossdev dev-lang/nasm dev-libs/libisoburn \
    sys-boot/grub sys-fs/mtools app-emulation/qemu dev-vcs/git

# 2. Use crossdev to build the cross-compiler (Gentoo way)
sudo crossdev --target i686-elf --stable --gcc 13.2.0 --binutils 2.41

# Or build manually (see Ubuntu section for the full steps)

# 3. Clone, build, run
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
export PATH="/opt/cross/bin:$PATH"  # or /usr/i686-elf/bin if using crossdev
make run
```

### NixOS / Nix

```bash
# 1. Using a nix-shell with all dependencies
# Create shell.nix in the project root or use this one-liner:
nix-shell -p gnumake nasm xorriso grub2 mtools qemu git \
    pkgsCross.i686-embedded.buildPackages.gcc \
    pkgsCross.i686-embedded.buildPackages.binutils

# 2. Inside the nix-shell:
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
make run

# Alternative: build cross-compiler manually (see Ubuntu section)
```

### Alpine Linux

```bash
# 1. Install dependencies
sudo apk add build-base gmp-dev mpfr-dev mpc1-dev texinfo \
    nasm xorriso grub grub-bios mtools qemu-system-i386 git curl bash

# 2. Build cross-compiler (same steps as Ubuntu step 2)
# NOTE: Alpine uses musl libc — the cross-compiler builds fine,
# it just compiles for bare-metal i686-elf anyway.

# 3. Clone, build, run
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
export PATH="/opt/cross/bin:$PATH"
make run
```

---

### macOS (Intel & Apple Silicon)

```bash
# 1. Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 2. Install dependencies
brew install nasm xorriso qemu git gmp mpfr libmpc mtools

# 3. Install GRUB for i386 target
# macOS doesn't package grub-mkrescue by default.
# Option A: Build GRUB from source
curl -LO https://ftp.gnu.org/gnu/grub/grub-2.12.tar.xz
tar xf grub-2.12.tar.xz && cd grub-2.12
./configure --target=i386 --prefix=/usr/local \
    --disable-werror --disable-efiemu
make -j$(sysctl -n hw.ncpu)
sudo make install
cd ..

# Option B: Use a pre-built GRUB (community taps)
# brew install --cask grub  (check availability)

# 4. Build the cross-compiler
export PREFIX="/usr/local/opt/cross"
export TARGET="i686-elf"
export PATH="$PREFIX/bin:$PATH"
sudo mkdir -p $PREFIX

mkdir -p /tmp/cross-build && cd /tmp/cross-build

# binutils
curl -LO https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz
tar xf binutils-2.41.tar.xz
mkdir build-binutils && cd build-binutils
../binutils-2.41/configure --target=$TARGET --prefix=$PREFIX \
    --with-sysroot --disable-nls --disable-werror
make -j$(sysctl -n hw.ncpu)
sudo make install
cd ..

# GCC
curl -LO https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz
tar xf gcc-13.2.0.tar.xz
mkdir build-gcc && cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix=$PREFIX \
    --disable-nls --enable-languages=c --without-headers \
    --with-gmp=$(brew --prefix gmp) \
    --with-mpfr=$(brew --prefix mpfr) \
    --with-mpc=$(brew --prefix libmpc)
make -j$(sysctl -n hw.ncpu) all-gcc all-target-libgcc
sudo make install-gcc install-target-libgcc
cd /tmp && rm -rf cross-build

# 5. Add to PATH (add to ~/.zshrc for persistence)
echo 'export PATH="/usr/local/opt/cross/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc

# 6. Clone, build, run
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
make run
```

> **Note for Apple Silicon (M1/M2/M3/M4):** The cross-compiler produces i686-elf binaries regardless of your host CPU. QEMU handles the x86 emulation transparently. Build times may be slightly longer due to Rosetta or native ARM compilation of the cross-tools, but the resulting AstraOS binary is identical.

---

### Windows

There are three approaches for building on Windows, listed from easiest to most involved:

#### Option A: WSL2 (Recommended)

Windows Subsystem for Linux gives you a full Linux environment. This is the easiest path.

```powershell
# 1. Install WSL2 (run in PowerShell as Administrator)
wsl --install -d Ubuntu

# 2. Restart your computer, then open "Ubuntu" from the Start menu

# 3. Inside WSL2, follow the Ubuntu/Debian instructions above exactly.
#    Everything works identically to native Linux.
```

To access the graphical QEMU window from WSL2:
- **WSL2 on Windows 11:** GUI apps work out of the box via WSLg
- **WSL2 on Windows 10:** Install an X server like VcXsrv or X410, then `export DISPLAY=:0`

#### Option B: MSYS2

```powershell
# 1. Download and install MSYS2 from https://www.msys2.org/

# 2. Open "MSYS2 UCRT64" terminal

# 3. Install dependencies
pacman -Syu
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain \
    nasm xorriso grub mtools git curl \
    gmp-devel mpfr-devel mpc-devel texinfo

# 4. Install QEMU for Windows
# Download from https://www.qemu.org/download/#windows
# Or: pacman -S mingw-w64-ucrt-x86_64-qemu

# 5. Build the cross-compiler (same steps as Linux, inside MSYS2)
export PREFIX="/opt/cross"
export TARGET="i686-elf"
export PATH="$PREFIX/bin:$PATH"
mkdir -p $PREFIX

# ... (follow the binutils + GCC build steps from Ubuntu section)

# 6. Clone, build, run
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
make run
```

#### Option C: Docker (Any Windows Version)

```powershell
# 1. Install Docker Desktop for Windows
# https://www.docker.com/products/docker-desktop/

# 2. Pull a Linux image and build inside it
docker run -it --rm -v ${PWD}:/work ubuntu:24.04 bash

# Inside the container, follow the Ubuntu instructions:
apt update && apt install -y build-essential nasm xorriso grub-pc-bin \
    grub-common mtools qemu-system-x86 git curl bison flex \
    libgmp-dev libmpc-dev libmpfr-dev texinfo
# ... (build cross-compiler, clone repo, make iso)

# 3. Copy the ISO out and run in QEMU on your host:
# qemu-system-i386 -cdrom build/astraos.iso -m 256M -vga std
```

---

### FreeBSD

```bash
# 1. Install dependencies
sudo pkg install nasm xorriso grub2-bhyve mtools qemu git curl \
    gmake gmp mpfr mpc texinfo

# 2. Build the cross-compiler
# NOTE: Use gmake instead of make on FreeBSD
export PREFIX="/usr/local/cross"
export TARGET="i686-elf"
export PATH="$PREFIX/bin:$PATH"
sudo mkdir -p $PREFIX

mkdir -p /tmp/cross-build && cd /tmp/cross-build
curl -LO https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz
curl -LO https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz

tar xf binutils-2.41.tar.xz
mkdir build-binutils && cd build-binutils
../binutils-2.41/configure --target=$TARGET --prefix=$PREFIX \
    --with-sysroot --disable-nls --disable-werror
gmake -j$(sysctl -n hw.ncpu) && sudo gmake install && cd ..

tar xf gcc-13.2.0.tar.xz
mkdir build-gcc && cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix=$PREFIX \
    --disable-nls --enable-languages=c --without-headers
gmake -j$(sysctl -n hw.ncpu) all-gcc all-target-libgcc
sudo gmake install-gcc install-target-libgcc
cd /tmp && rm -rf cross-build

# 3. Clone and build (use gmake on FreeBSD)
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
export PATH="/usr/local/cross/bin:$PATH"
gmake run
```

### OpenBSD

```bash
# 1. Install dependencies
pkg_add nasm xorriso grub2 mtools qemu git curl gmake \
    gmp mpfr libmpc texinfo bison

# 2. Build cross-compiler (same as FreeBSD, use gmake)
# ... (see FreeBSD section)

# 3. Clone, build with gmake
git clone https://github.com/D3LTA2033/AstraOS.git
cd AstraOS
export PATH="/usr/local/cross/bin:$PATH"
gmake run
```

---

### Docker (Any Platform)

If you don't want to install anything on your host system, use Docker:

```bash
# Quick one-liner to build and get the ISO
docker run --rm -v $(pwd)/output:/output ubuntu:24.04 bash -c '
    apt update && apt install -y build-essential nasm xorriso grub-pc-bin \
        grub-common mtools git curl bison flex libgmp-dev libmpc-dev \
        libmpfr-dev texinfo &&
    export PREFIX=/opt/cross TARGET=i686-elf PATH=/opt/cross/bin:$PATH &&
    mkdir -p /opt/cross /tmp/build && cd /tmp/build &&
    curl -sL https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz | tar xJ &&
    mkdir bb && cd bb &&
    ../binutils-2.41/configure --target=$TARGET --prefix=$PREFIX \
        --with-sysroot --disable-nls --disable-werror &&
    make -j$(nproc) && make install && cd .. &&
    curl -sL https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz | tar xJ &&
    mkdir bg && cd bg &&
    ../gcc-13.2.0/configure --target=$TARGET --prefix=$PREFIX \
        --disable-nls --enable-languages=c --without-headers &&
    make -j$(nproc) all-gcc all-target-libgcc &&
    make install-gcc install-target-libgcc && cd / &&
    git clone https://github.com/D3LTA2033/AstraOS.git && cd AstraOS &&
    make iso && cp build/astraos.iso /output/
'

# Run the ISO with your local QEMU
qemu-system-i386 -cdrom output/astraos.iso -m 256M -vga std
```

---

### Running on Real Hardware

AstraOS can boot on any BIOS-compatible x86 machine:

```bash
# 1. Build the ISO
make iso

# 2. Write to a USB drive (replace /dev/sdX with your USB device)
# WARNING: This will ERASE the USB drive!
sudo dd if=build/astraos.iso of=/dev/sdX bs=4M status=progress
sync

# 3. Boot from USB
# - Insert the USB drive
# - Enter BIOS/boot menu (usually F2, F12, DEL, or ESC at startup)
# - Select the USB drive as boot device
# - AstraOS will boot into the installer wizard
```

**Hardware requirements:**
- Any x86 CPU (Pentium or newer — integer-only graphics, no FPU/SSE required)
- 32MB+ RAM (256MB recommended)
- BIOS boot support (not UEFI-only machines; most PCs support Legacy/CSM mode)
- VESA-compatible graphics (virtually all GPUs since 1998)
- PS/2 keyboard and mouse (or USB with BIOS PS/2 emulation, which most BIOSes provide)

**Tested environments:** QEMU, VirtualBox, VMware Workstation, Bochs, and bare-metal Lenovo ThinkPad / Dell OptiPlex / HP ProDesk systems.

---

### Verifying Your Installation

After building, verify everything works:

```bash
# Check cross-compiler
i686-elf-gcc --version          # Should show i686-elf target
i686-elf-ld --version           # Should show i686-elf target

# Check tools
nasm --version
grub-mkrescue --version
qemu-system-i386 --version

# Build and run
cd AstraOS
make clean && make iso          # Should produce build/astraos.iso (~4MB)
make run                        # Should boot into the installer wizard

# The first-boot installer wizard runs automatically:
#   - Press Enter to advance through each step
#   - Use arrow keys or mouse to select a theme
#   - After installation, the desktop launches with all 4 applications
#   - The VS Code-like editor, terminal, file manager, and settings app
#     will appear on the glassmorphism desktop
```

### Build Commands Reference

```bash
export PATH="/opt/cross/bin:$PATH"

make            # Build kernel ELF binary (build/astra.kernel)
make iso        # Build bootable GRUB ISO (build/astraos.iso)
make run        # Build + boot in QEMU (graphical, serial on stdio)
make run-nographic  # Build + boot headless (serial output only)
make clean      # Remove all build artifacts
```

### Troubleshooting

| Problem | Solution |
|---------|----------|
| `i686-elf-gcc: command not found` | Add cross-compiler to PATH: `export PATH="/opt/cross/bin:$PATH"` |
| `grub-mkrescue: command not found` | Install GRUB tools: `apt install grub-pc-bin grub-common` (Debian) or `pacman -S grub` (Arch) |
| `xorriso: command not found` | Install xorriso: `apt install xorriso` / `brew install xorriso` |
| `make: *** No rule to make target` | Ensure you're in the AstraOS root directory |
| QEMU window is black | Wait 5-8 seconds for the installer to render; press Enter if stuck |
| QEMU: "No bootable device" | Ensure `make iso` completed successfully and the ISO path is correct |
| Cross-compiler build fails on macOS | Pass Homebrew lib paths: `--with-gmp=$(brew --prefix gmp)` etc. |
| WSL2: no QEMU window | Windows 11: works via WSLg. Windows 10: install VcXsrv and set `DISPLAY=:0` |
| FreeBSD/OpenBSD: `make` errors | Use `gmake` (GNU Make) instead of BSD `make` |

---

## Development Roadmap

| Phase | Name | Status | Description |
|-------|------|--------|-------------|
| 1 | Boot Foundation | **Complete** | Multiboot, GDT, VGA console, GRUB ISO boot |
| 2 | Memory + Interrupts | **Complete** | GDT/IDT/TSS, PIC, PIT, keyboard, PMM |
| 3 | Paging + Heap | **Complete** | Two-level paging, identity map, kernel heap |
| 4 | Tasking + Scheduler | **Complete** | Context switching, preemption, task lifecycle |
| 5 | User Mode + Syscalls | **Complete** | Ring 3, int 0x80 dispatch, user program loading |
| 6 | Filesystem + Drivers | **Complete** | VFS, ramfs, devfs, procfs, initramfs, serial, RTC, mouse |
| 7 | Security Core | **Complete** | Capability-based access control, signals, pipes |
| 8 | Graphical Desktop | **Complete** | Framebuffer, compositing WM, glass effects, mouse cursor |
| 9 | Applications | **Complete** | Code editor, terminal, file manager, settings |
| 10 | Astracor Language | **Complete** | Interpreter, stdlib, module system, syntax highlighting |
| 11 | Theme Engine | **Complete** | 12 themes, 7 wallpapers, live switching |
| 12 | Networking | Planned | NE2000/RTL8139 driver, TCP/IP stack |
| 13 | Persistent Storage | Planned | ATA/IDE driver, ext2-like filesystem |
| 14 | x86_64 Port | Planned | Long mode, 64-bit address space |

---

## Codebase Statistics

| Metric | Count |
|--------|-------|
| Total lines of code | **12,740+** |
| C source files | **32** |
| Header files | **34** |
| Assembly files | **6** |
| Kernel binary size | **~300 KB** |
| ISO image size | **~4 MB** |
| External dependencies | **0** |
| libc functions used | **0** |

---

## Coding Conventions

- **C11 standard**, compiled with `-Wall -Wextra -Werror -ffreestanding -nostdlib`
- All kernel utility functions prefixed with `k` (`kstrlen`, `kmemset`, `kmalloc`)
- Architecture-specific code strictly isolated under `arch/`
- Driver interfaces in `include/drivers/`, implementations in `kernel/drivers/`
- No dynamic allocation during interrupt handling
- All interrupt handlers send EOI before returning
- User-space pointers validated before kernel dereference
- Integer-only math in all graphics code (no FPU)
- Theme colors accessed through `theme_current()` — never hardcoded in app code

---

## License

MIT License. See [LICENSE](LICENSE) for details.
