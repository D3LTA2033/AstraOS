/* ==========================================================================
 * AstraOS - Real-Time Clock (RTC) Driver
 * ==========================================================================
 * Reads the MC146818-compatible CMOS RTC via I/O ports 0x70/0x71.
 * Handles both BCD and binary modes. Waits for update-in-progress
 * flag to clear before reading to ensure consistent values.
 * ========================================================================== */

#include <drivers/rtc.h>
#include <kernel/devfs.h>
#include <kernel/kstring.h>

/* CMOS I/O ports */
#define CMOS_ADDR  0x70
#define CMOS_DATA  0x71

/* CMOS registers */
#define RTC_SECONDS   0x00
#define RTC_MINUTES   0x02
#define RTC_HOURS     0x04
#define RTC_WEEKDAY   0x06
#define RTC_DAY       0x07
#define RTC_MONTH     0x08
#define RTC_YEAR      0x09
#define RTC_STATUS_A  0x0A
#define RTC_STATUS_B  0x0B

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int rtc_updating(void)
{
    return cmos_read(RTC_STATUS_A) & 0x80;
}

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

void rtc_init(void)
{
    /* Nothing special needed — the RTC runs continuously */
}

void rtc_read(rtc_time_t *t)
{
    /* Wait for any update to finish */
    while (rtc_updating())
        ;

    t->second  = cmos_read(RTC_SECONDS);
    t->minute  = cmos_read(RTC_MINUTES);
    t->hour    = cmos_read(RTC_HOURS);
    t->day     = cmos_read(RTC_DAY);
    t->month   = cmos_read(RTC_MONTH);
    t->year    = cmos_read(RTC_YEAR);
    t->weekday = cmos_read(RTC_WEEKDAY);

    uint8_t status_b = cmos_read(RTC_STATUS_B);

    /* If BCD mode (bit 2 of status B is clear), convert to binary */
    if (!(status_b & 0x04)) {
        t->second  = bcd_to_bin(t->second);
        t->minute  = bcd_to_bin(t->minute);
        t->hour    = bcd_to_bin(t->hour & 0x7F) | (t->hour & 0x80);
        t->day     = bcd_to_bin(t->day);
        t->month   = bcd_to_bin(t->month);
        t->year    = bcd_to_bin((uint8_t)t->year);
    }

    /* Convert 12-hour to 24-hour if needed */
    if (!(status_b & 0x02) && (t->hour & 0x80)) {
        t->hour = ((t->hour & 0x7F) + 12) % 24;
    }

    /* Assume century is 20xx */
    t->year += 2000;
}

/* VFS read handler for /dev/rtc */
static uint32_t rtc_vfs_read(vfs_node_t *node, uint32_t offset,
                             uint32_t size, uint8_t *buffer)
{
    (void)node;
    rtc_time_t t;
    rtc_read(&t);

    char tmp[64];
    uint32_t len = 0;

    /* Format: YYYY-MM-DD HH:MM:SS\n */
    /* Year */
    tmp[len++] = '0' + (char)(t.year / 1000);
    tmp[len++] = '0' + (char)((t.year / 100) % 10);
    tmp[len++] = '0' + (char)((t.year / 10) % 10);
    tmp[len++] = '0' + (char)(t.year % 10);
    tmp[len++] = '-';
    tmp[len++] = '0' + (char)(t.month / 10);
    tmp[len++] = '0' + (char)(t.month % 10);
    tmp[len++] = '-';
    tmp[len++] = '0' + (char)(t.day / 10);
    tmp[len++] = '0' + (char)(t.day % 10);
    tmp[len++] = ' ';
    tmp[len++] = '0' + (char)(t.hour / 10);
    tmp[len++] = '0' + (char)(t.hour % 10);
    tmp[len++] = ':';
    tmp[len++] = '0' + (char)(t.minute / 10);
    tmp[len++] = '0' + (char)(t.minute % 10);
    tmp[len++] = ':';
    tmp[len++] = '0' + (char)(t.second / 10);
    tmp[len++] = '0' + (char)(t.second % 10);
    tmp[len++] = '\n';

    if (offset >= len) return 0;
    uint32_t avail = len - offset;
    if (size > avail) size = avail;
    kmemcpy(buffer, tmp + offset, size);
    return size;
}

void rtc_register_device(void)
{
    devfs_register_chardev("rtc", rtc_vfs_read, NULL, NULL, NULL);
}
