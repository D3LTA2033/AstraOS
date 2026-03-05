/* ==========================================================================
 * AstraOS - Initramfs (USTAR Tar Archive Parser)
 * ==========================================================================
 * Parses a USTAR-format tar archive and creates VFS entries for each file
 * and directory found. Files are stored as ramfs nodes.
 *
 * Tar header format: 512-byte blocks, with file data following in
 * ceil(size/512) blocks. Directories have type '5', files have type '0'.
 * ========================================================================== */

#include <kernel/initramfs.h>
#include <kernel/vfs.h>
#include <kernel/ramfs.h>
#include <kernel/kstring.h>
#include <drivers/serial.h>

/* USTAR tar header (512 bytes) */
typedef struct __attribute__((packed)) {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];      /* Octal ASCII */
    char mtime[12];
    char checksum[8];
    char typeflag;      /* '0'=file, '5'=directory */
    char linkname[100];
    char magic[6];      /* "ustar" */
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} tar_header_t;

/* Parse an octal ASCII string to uint32_t */
static uint32_t oct_to_uint(const char *s, int len)
{
    uint32_t val = 0;
    for (int i = 0; i < len && s[i] >= '0' && s[i] <= '7'; i++)
        val = val * 8 + (uint32_t)(s[i] - '0');
    return val;
}

/* Find or create intermediate directories along a path */
static vfs_node_t *ensure_parent(const char *path)
{
    vfs_node_t *cur = vfs_root();
    char component[VFS_NAME_MAX + 1];

    const char *p = path;
    if (*p == '/') p++;

    while (*p) {
        /* Extract next component */
        int i = 0;
        while (*p && *p != '/' && i < VFS_NAME_MAX)
            component[i++] = *p++;
        component[i] = '\0';

        if (*p == '/') p++;

        /* If there's more path left, this is a directory component */
        if (*p != '\0') {
            vfs_node_t *child = vfs_finddir(cur, component);
            if (!child) {
                child = ramfs_create_dir(component);
                vfs_add_child(cur, child);
            }
            cur = child;
        }
        /* Last component is the filename — return its parent */
    }
    return cur;
}

/* Extract just the filename from a path */
static const char *basename(const char *path)
{
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' && *(p + 1) != '\0')
            last = p + 1;
    }
    return last;
}

int initramfs_load(void *base, uint32_t size)
{
    uint8_t *ptr = (uint8_t *)base;
    uint8_t *end = ptr + size;
    int count = 0;

    serial_write("[INITRAMFS] Loading tar archive...\r\n");

    while (ptr + 512 <= end) {
        tar_header_t *hdr = (tar_header_t *)ptr;

        /* Two consecutive zero blocks = end of archive */
        if (hdr->name[0] == '\0')
            break;

        uint32_t file_size = oct_to_uint(hdr->size, 12);
        uint8_t *data = ptr + 512;

        /* Strip leading "./" if present */
        const char *name = hdr->name;
        if (name[0] == '.' && name[1] == '/')
            name += 2;
        if (name[0] == '\0') {
            /* Skip empty names */
            ptr += 512 + ((file_size + 511) & ~511);
            continue;
        }

        /* Remove trailing slash for directories */
        char clean_name[256];
        uint32_t nlen = kstrlen(name);
        if (nlen >= sizeof(clean_name)) nlen = sizeof(clean_name) - 1;
        kmemcpy(clean_name, name, nlen);
        clean_name[nlen] = '\0';
        if (nlen > 0 && clean_name[nlen - 1] == '/')
            clean_name[--nlen] = '\0';

        if (hdr->typeflag == '5' || (hdr->typeflag == '\0' && file_size == 0 && nlen > 0)) {
            /* Directory */
            vfs_node_t *parent = ensure_parent(clean_name);
            const char *dname = basename(clean_name);
            if (!vfs_finddir(parent, dname)) {
                vfs_node_t *dir = ramfs_create_dir(dname);
                vfs_add_child(parent, dir);
            }
            serial_write("  [DIR]  ");
            serial_write(clean_name);
            serial_write("\r\n");
        } else if (hdr->typeflag == '0' || hdr->typeflag == '\0') {
            /* Regular file */
            vfs_node_t *parent = ensure_parent(clean_name);
            const char *fname = basename(clean_name);
            vfs_node_t *file = ramfs_create_file(fname, data, file_size);
            vfs_add_child(parent, file);
            serial_write("  [FILE] ");
            serial_write(clean_name);
            serial_write("\r\n");
            count++;
        }

        /* Advance past header + data (rounded to 512-byte blocks) */
        ptr += 512 + ((file_size + 511) & ~511);
    }

    serial_write("[INITRAMFS] Loaded ");
    /* Simple decimal print */
    char buf[12];
    int i = 0;
    int tmp = count;
    if (tmp == 0) buf[i++] = '0';
    else { while (tmp > 0) { buf[i++] = '0' + (char)(tmp % 10); tmp /= 10; } }
    for (int j = i - 1; j >= 0; j--) serial_putchar(buf[j]);
    serial_write(" files\r\n");

    return count;
}
