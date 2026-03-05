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

## Building

### Prerequisites

| Tool | Purpose |
|------|---------|
| `i686-elf-gcc` | Cross-compiler (C11, freestanding) |
| `i686-elf-ld` | Linker |
| `nasm` | Assembler (elf32) |
| `grub-mkrescue` + `xorriso` | ISO generation |
| `qemu-system-i386` | Testing |

### Building the Cross-Compiler

```bash
export PREFIX="/opt/cross"
export TARGET="i686-elf"
export PATH="$PREFIX/bin:$PATH"

# binutils
tar xf binutils-2.41.tar.xz && cd binutils-2.41
./configure --target=$TARGET --prefix=$PREFIX --with-sysroot --disable-nls --disable-werror
make -j$(nproc) && make install && cd ..

# GCC (C only)
tar xf gcc-13.2.0.tar.xz && cd gcc-13.2.0
./configure --target=$TARGET --prefix=$PREFIX --disable-nls --enable-languages=c --without-headers
make -j$(nproc) all-gcc all-target-libgcc
make install-gcc install-target-libgcc
```

### Build Commands

```bash
export PATH="/opt/cross/bin:$PATH"

make            # Build kernel ELF
make iso        # Build bootable GRUB ISO
make run        # Build + boot in QEMU (graphical, serial on stdio)
make clean      # Remove all build artifacts
```

### Running

```bash
# Standard boot — graphical desktop (1024x768x32bpp)
make run

# Headless boot — serial output only
make run-nographic

# The first-boot installer wizard will run automatically.
# Navigate with Enter, arrow keys, and mouse clicks.
# After installation, the desktop launches with all 4 applications.
```

QEMU is configured with 256MB RAM, CD-ROM boot, and VGA standard output. The desktop initializes a 1024x768 VESA framebuffer with double buffering.

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
