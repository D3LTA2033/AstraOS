/* ==========================================================================
 * AstraOS - Kernel Entry Point
 * ==========================================================================
 * Graphical desktop environment for programming & testing.
 * Initializes framebuffer, window manager, and Astracor language.
 * ========================================================================== */

#include <kernel/types.h>
#include <kernel/multiboot.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/pic.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/page_fault.h>
#include <kernel/kheap.h>
#include <kernel/task.h>
#include <kernel/scheduler.h>
#include <kernel/syscall.h>
#include <kernel/usermode.h>
#include <kernel/vfs.h>
#include <kernel/devfs.h>
#include <kernel/ramfs.h>
#include <kernel/capability.h>
#include <kernel/kstring.h>
#include <kernel/pipe.h>
#include <kernel/initramfs.h>
#include <kernel/signal.h>
#include <kernel/gui.h>
#include <kernel/apps.h>
#include <kernel/installer.h>
#include <drivers/vga.h>
#include <drivers/pit.h>
#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <drivers/rtc.h>
#include <drivers/framebuffer.h>
#include <drivers/mouse.h>

/* Embedded user programs (linked as binary blobs) */
extern char _binary_user_test_user_bin_start[];
extern char _binary_user_test_user_bin_end[];
extern char _binary_user_hello_bin_start[];
extern char _binary_user_hello_bin_end[];
extern char _binary_user_fib_bin_start[];
extern char _binary_user_fib_bin_end[];

static void print_dec(uint32_t val)
{
    if (val == 0) { vga_putchar('0'); return; }
    char buf[12];
    int i = 0;
    while (val > 0) { buf[i++] = '0' + (char)(val % 10); val /= 10; }
    for (int j = i - 1; j >= 0; j--) vga_putchar(buf[j]);
}

/* --- Console device (/dev/console) --- */

static uint32_t console_vfs_write(vfs_node_t *node, uint32_t offset,
                                  uint32_t size, const uint8_t *buffer)
{
    (void)node; (void)offset;
    for (uint32_t i = 0; i < size; i++)
        vga_putchar((char)buffer[i]);
    return size;
}

static uint32_t console_vfs_read(vfs_node_t *node, uint32_t offset,
                                 uint32_t size, uint8_t *buffer)
{
    (void)node; (void)offset;
    uint32_t count = 0;

    while (count < size) {
        int c = keyboard_getchar();
        if (c < 0) {
            __asm__ volatile ("sti; hlt");
            continue;
        }

        if (c == '\b') {
            if (count > 0) {
                count--;
                vga_putchar('\b');
            }
            continue;
        }

        vga_putchar((char)c);
        buffer[count++] = (uint8_t)c;
        if (c == '\n')
            break;
    }

    return count;
}

/* --- Procfs helpers --- */

static void fmt_dec(char *buf, uint32_t *pos, uint32_t val)
{
    if (val == 0) { buf[(*pos)++] = '0'; return; }
    char tmp[12];
    int i = 0;
    while (val > 0) { tmp[i++] = '0' + (char)(val % 10); val /= 10; }
    for (int j = i - 1; j >= 0; j--) buf[(*pos)++] = tmp[j];
}

static void fmt_str(char *buf, uint32_t *pos, const char *s)
{
    while (*s) buf[(*pos)++] = *s++;
}

static uint32_t procfs_meminfo_read(vfs_node_t *node, uint32_t offset,
                                    uint32_t size, uint8_t *buffer)
{
    (void)node;
    char tmp[256];
    uint32_t len = 0;

    size_t heap_total, heap_used, heap_free;
    kheap_stats(&heap_total, &heap_used, &heap_free);

    fmt_str(tmp, &len, "PMM Frames: ");
    fmt_dec(tmp, &len, pmm_free_frames());
    fmt_str(tmp, &len, "/");
    fmt_dec(tmp, &len, pmm_total_frames());
    fmt_str(tmp, &len, " free (");
    fmt_dec(tmp, &len, (pmm_free_frames() * 4) / 1024);
    fmt_str(tmp, &len, " MB)\nHeap: ");
    fmt_dec(tmp, &len, (uint32_t)(heap_used / 1024));
    fmt_str(tmp, &len, " KB used, ");
    fmt_dec(tmp, &len, (uint32_t)(heap_free / 1024));
    fmt_str(tmp, &len, " KB free\n");

    if (offset >= len) return 0;
    uint32_t avail = len - offset;
    if (size > avail) size = avail;
    kmemcpy(buffer, tmp + offset, size);
    return size;
}

static uint32_t procfs_uptime_read(vfs_node_t *node, uint32_t offset,
                                   uint32_t size, uint8_t *buffer)
{
    (void)node;
    char tmp[64];
    uint32_t len = 0;
    uint32_t ticks = pit_get_ticks();
    uint32_t secs = ticks / 100;

    fmt_dec(tmp, &len, secs);
    fmt_str(tmp, &len, " seconds (");
    fmt_dec(tmp, &len, ticks);
    fmt_str(tmp, &len, " ticks)\n");

    if (offset >= len) return 0;
    uint32_t avail = len - offset;
    if (size > avail) size = avail;
    kmemcpy(buffer, tmp + offset, size);
    return size;
}

static const char *state_str(task_state_t s)
{
    switch (s) {
    case TASK_READY:   return "READY  ";
    case TASK_RUNNING: return "RUNNING";
    case TASK_BLOCKED: return "BLOCKED";
    case TASK_DEAD:    return "DEAD   ";
    default:           return "???    ";
    }
}

static uint32_t procfs_tasks_read(vfs_node_t *node, uint32_t offset,
                                  uint32_t size, uint8_t *buffer)
{
    (void)node;
    char tmp[1024];
    uint32_t len = 0;

    fmt_str(tmp, &len, "PID  STATE    CAPS\n");
    fmt_str(tmp, &len, "---  -------  ----------\n");

    for (uint32_t i = 1; i <= MAX_TASKS && len < 900; i++) {
        task_t *t = task_find(i);
        if (!t) continue;
        if (t->state == TASK_DEAD) continue;

        fmt_dec(tmp, &len, t->id);
        if (t->id < 10) fmt_str(tmp, &len, "    ");
        else if (t->id < 100) fmt_str(tmp, &len, "   ");
        else fmt_str(tmp, &len, "  ");

        fmt_str(tmp, &len, state_str(t->state));
        fmt_str(tmp, &len, "  0x");

        const char hex[] = "0123456789ABCDEF";
        for (int j = 28; j >= 0; j -= 4)
            tmp[len++] = hex[(t->capabilities >> j) & 0xF];

        fmt_str(tmp, &len, "\n");
    }

    if (offset >= len) return 0;
    uint32_t avail = len - offset;
    if (size > avail) size = avail;
    kmemcpy(buffer, tmp + offset, size);
    return size;
}

static uint32_t procfs_cpuinfo_read(vfs_node_t *node, uint32_t offset,
                                    uint32_t size, uint8_t *buffer)
{
    (void)node;
    char tmp[256];
    uint32_t len = 0;

    uint32_t ebx_val, ecx_val, edx_val;
    __asm__ volatile ("cpuid"
        : "=b"(ebx_val), "=c"(ecx_val), "=d"(edx_val)
        : "a"(0));

    fmt_str(tmp, &len, "Vendor: ");
    char *p = (char *)&ebx_val;
    for (int i = 0; i < 4; i++) tmp[len++] = p[i];
    p = (char *)&edx_val;
    for (int i = 0; i < 4; i++) tmp[len++] = p[i];
    p = (char *)&ecx_val;
    for (int i = 0; i < 4; i++) tmp[len++] = p[i];
    fmt_str(tmp, &len, "\nArch: i686 (x86 32-bit)\n");

    if (offset >= len) return 0;
    uint32_t avail = len - offset;
    if (size > avail) size = avail;
    kmemcpy(buffer, tmp + offset, size);
    return size;
}

void kernel_main(uint32_t magic, uint32_t mboot_addr)
{
    multiboot_info_t *mboot = (multiboot_info_t *)mboot_addr;

    /* Early text-mode VGA for boot messages */
    vga_init();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        vga_write("FATAL: Not loaded by a Multiboot bootloader!\n");
        return;
    }

    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    vga_write("========================================\n");
    vga_write("  AstraOS v2.0.0 - AstraCore Kernel\n");
    vga_write("  Graphical Desktop Environment\n");
    vga_write("========================================\n\n");

    vga_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));

    /* Phase 2 - Core hardware */
    gdt_init();
    vga_write("[OK] GDT initialized\n");
    idt_init();
    vga_write("[OK] IDT initialized\n");
    pic_init();
    vga_write("[OK] PIC remapped\n");
    pit_init();
    vga_write("[OK] PIT timer at 100 Hz\n");
    keyboard_init();
    vga_write("[OK] Keyboard driver loaded\n");
    pmm_init(mboot);

    /* Phase 3 - Memory management */
    page_fault_init();
    vmm_init();
    kheap_init();
    vga_write("[OK] Kernel heap initialized\n");

    /* Phase 4 - Tasking */
    scheduler_init();
    vga_write("[OK] Scheduler initialized\n");

    /* Phase 5 - User mode */
    syscall_init();
    vga_write("[OK] Syscall interface registered\n");
    usermode_init();

    /* Phase 6 - Filesystem + drivers */
    serial_init();
    serial_write("[AstraOS] Serial port active\r\n");

    /* Phase 7 - Security */
    security_init();

    vfs_init();
    devfs_init();

    devfs_register_chardev("console",
                           console_vfs_read,
                           console_vfs_write,
                           NULL, NULL);
    serial_register_device();
    rtc_init();
    rtc_register_device();

    /* Create /etc */
    vfs_node_t *etc = ramfs_create_dir("etc");
    vfs_add_child(vfs_root(), etc);
    const char *hostname = "astra";
    vfs_add_child(etc, ramfs_create_file("hostname", hostname, kstrlen(hostname)));
    const char *version = "AstraOS 2.0.0 (AstraCore GUI)";
    vfs_add_child(etc, ramfs_create_file("version", version, kstrlen(version)));

    /* Create /proc */
    vfs_node_t *proc = vfs_create_node("proc", VFS_DIRECTORY);
    vfs_add_child(vfs_root(), proc);

    vfs_node_t *meminfo_node = vfs_create_node("meminfo", VFS_FILE);
    meminfo_node->read = procfs_meminfo_read;
    vfs_add_child(proc, meminfo_node);

    vfs_node_t *uptime_node = vfs_create_node("uptime", VFS_FILE);
    uptime_node->read = procfs_uptime_read;
    vfs_add_child(proc, uptime_node);

    vfs_node_t *tasks_node = vfs_create_node("tasks", VFS_FILE);
    tasks_node->read = procfs_tasks_read;
    vfs_add_child(proc, tasks_node);

    vfs_node_t *cpuinfo_node = vfs_create_node("cpuinfo", VFS_FILE);
    cpuinfo_node->read = procfs_cpuinfo_read;
    vfs_add_child(proc, cpuinfo_node);

    /* Create /bin with embedded user programs */
    vfs_node_t *bin = ramfs_create_dir("bin");
    vfs_add_child(vfs_root(), bin);

    size_t hello_size = (size_t)(_binary_user_hello_bin_end -
                                  _binary_user_hello_bin_start);
    vfs_add_child(bin, ramfs_create_file("hello",
                  _binary_user_hello_bin_start, (uint32_t)hello_size));

    size_t fib_size = (size_t)(_binary_user_fib_bin_end -
                                _binary_user_fib_bin_start);
    vfs_add_child(bin, ramfs_create_file("fib",
                  _binary_user_fib_bin_start, (uint32_t)fib_size));

    vga_write("[OK] VFS + devfs + procfs + /bin ready\n");

    /* Load initramfs from GRUB module if present */
    if (mboot->flags & MULTIBOOT_FLAG_MODS && mboot->mods_count > 0) {
        multiboot_module_t *mod = (multiboot_module_t *)mboot->mods_addr;
        uint32_t mod_size = mod->mod_end - mod->mod_start;
        int nfiles = initramfs_load((void *)mod->mod_start, mod_size);
        if (nfiles >= 0) {
            vga_write("[OK] Initramfs: ");
            print_dec((uint32_t)nfiles);
            vga_write(" files loaded\n");
        }
    }

    /* --- Initialize graphical desktop --- */

    /* Initialize PS/2 mouse */
    mouse_init(1024, 768);
    vga_write("[OK] PS/2 mouse driver loaded\n");

    /* Initialize framebuffer from multiboot info */
    if (mboot->flags & MULTIBOOT_FLAG_FB) {
        fb_init(mboot);
        vga_write("[OK] Framebuffer: ");
        print_dec(mboot->framebuffer_width);
        vga_write("x");
        print_dec(mboot->framebuffer_height);
        vga_write("x");
        print_dec(mboot->framebuffer_bpp);
        vga_write("\n");

        /* Enable interrupts for installer input */
        __asm__ volatile ("sti");

        /* Run first-boot installation wizard */
        vga_write("[OK] Launching installation wizard...\n");
        installer_run();

        /* Config applied from installer */
        (void)installer_get_config();

        /* Initialize GUI window manager */
        gui_init();
        vga_write("[OK] GUI window manager initialized\n");

        /* Create default applications */
        app_editor_create(40, 40, 580, 500, "main.ac");
        app_terminal_create(50, 300, 500, 350);
        app_filemgr_create(640, 40, 340, 500);
        vga_write("[OK] Desktop applications launched\n");

        vga_set_color(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
        vga_write("\n[*] Desktop ready!\n");

        /* Enter GUI event loop */
        while (1) {
            gui_tick();
            __asm__ volatile ("hlt");
        }
    } else {
        /* Fallback: no framebuffer, run text-mode shell */
        vga_set_color(vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK));
        vga_write("[WARN] No framebuffer available, falling back to text shell\n");

        size_t user_size = (size_t)(_binary_user_test_user_bin_end -
                                    _binary_user_test_user_bin_start);
        vga_write("[OK] Shell binary: ");
        print_dec((uint32_t)user_size);
        vga_write(" bytes\n");

        uint32_t utid = usermode_task_create(
            _binary_user_test_user_bin_start, user_size);

        if (utid) {
            vga_write("[*] Shell spawned (task ");
            print_dec(utid);
            vga_write(") -> ring 3\n\n");
        } else {
            vga_set_color(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
            vga_write("[FAIL] Could not create shell task\n");
        }

        vga_set_color(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        __asm__ volatile ("sti");
        while (1)
            __asm__ volatile ("hlt");
    }
}
