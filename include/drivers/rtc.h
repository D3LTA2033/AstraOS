/* ==========================================================================
 * AstraOS - Real-Time Clock (RTC) Driver
 * ==========================================================================
 * Reads date and time from the CMOS RTC (MC146818 compatible).
 * Uses I/O ports 0x70 (index) and 0x71 (data).
 * ========================================================================== */

#ifndef _ASTRA_DRIVERS_RTC_H
#define _ASTRA_DRIVERS_RTC_H

#include <kernel/types.h>

typedef struct {
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
    uint8_t  weekday;
} rtc_time_t;

/* Initialize the RTC driver */
void rtc_init(void);

/* Read the current date and time */
void rtc_read(rtc_time_t *time);

/* Register /dev/rtc device node */
void rtc_register_device(void);

#endif /* _ASTRA_DRIVERS_RTC_H */
