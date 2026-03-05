/* ==========================================================================
 * AstraOS - x86 (i686) Linker Script
 * ==========================================================================
 * Controls the memory layout of the kernel binary.
 *
 * Key decisions:
 *   - Kernel loads at 1MB (0x100000). This is the standard location above
 *     the real-mode memory area and BIOS data structures.
 *   - Sections are page-aligned (4KB) to prepare for paging in Phase 3.
 *   - We export symbols for section boundaries so the kernel can discover
 *     its own memory layout (needed for memory management).
 * ========================================================================== */

ENTRY(_start)

SECTIONS
{
    /* Kernel starts at 1MB — above real-mode memory, BIOS, and VGA */
    . = 1M;

    /* Multiboot header must be early in the binary (within first 8KB) */
    .multiboot ALIGN(4K) :
    {
        *(.multiboot)
    }

    /* Executable code */
    .text ALIGN(4K) :
    {
        _text_start = .;
        *(.text)
        _text_end = .;
    }

    /* Read-only data (string literals, const arrays) */
    .rodata ALIGN(4K) :
    {
        _rodata_start = .;
        *(.rodata*)
        _rodata_end = .;
    }

    /* Initialized read-write data */
    .data ALIGN(4K) :
    {
        _data_start = .;
        *(.data)
        _data_end = .;
    }

    /* Zero-initialized data (BSS) — not stored in the binary */
    .bss ALIGN(4K) :
    {
        _bss_start = .;
        *(COMMON)
        *(.bss)
        _bss_end = .;
    }

    /* End of kernel image — used by memory allocator to know where
     * free memory begins */
    _kernel_end = .;
}
