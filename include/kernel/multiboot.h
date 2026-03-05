/* ==========================================================================
 * AstraOS - Multiboot 1 Information Structures
 * ==========================================================================
 * Structures matching the Multiboot 1 specification. GRUB populates these
 * and passes a pointer in EBX at boot. We use the memory map (flags bit 6)
 * to discover available physical RAM.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_MULTIBOOT_H
#define _ASTRA_KERNEL_MULTIBOOT_H

#include <kernel/types.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Flags bits indicating which fields are valid */
#define MULTIBOOT_FLAG_MEM      (1 << 0)    /* mem_lower, mem_upper */
#define MULTIBOOT_FLAG_MMAP     (1 << 6)    /* mmap_length, mmap_addr */

typedef struct __attribute__((packed)) {
    uint32_t flags;
    uint32_t mem_lower;         /* KB of lower memory (below 1MB) */
    uint32_t mem_upper;         /* KB of upper memory (above 1MB) */
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;       /* Size of memory map buffer */
    uint32_t mmap_addr;         /* Physical address of memory map */
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    /* Framebuffer info (if flags bit 12 is set) */
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  color_info[6];
} multiboot_info_t;

#define MULTIBOOT_FLAG_FB   (1 << 12)   /* framebuffer info valid */

/* Memory map entry (note: size field is NOT part of the 20-byte entry) */
typedef struct __attribute__((packed)) {
    uint32_t size;              /* Size of this entry (excluding this field) */
    uint64_t base_addr;         /* Physical start address */
    uint64_t length;            /* Length in bytes */
    uint32_t type;              /* 1=available, 2=reserved, 3=ACPI, etc. */
} multiboot_mmap_entry_t;

/* Multiboot module entry */
typedef struct __attribute__((packed)) {
    uint32_t mod_start;     /* Physical start address of module */
    uint32_t mod_end;       /* Physical end address of module */
    uint32_t cmdline;       /* Module command line string (physical addr) */
    uint32_t pad;           /* Reserved, must be 0 */
} multiboot_module_t;

#define MULTIBOOT_FLAG_MODS     (1 << 3)    /* mods_count, mods_addr */
#define MULTIBOOT_MMAP_AVAILABLE 1

#endif /* _ASTRA_KERNEL_MULTIBOOT_H */
