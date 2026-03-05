/* AstraOS - First-Boot Installation Wizard */

#ifndef _ASTRA_KERNEL_INSTALLER_H
#define _ASTRA_KERNEL_INSTALLER_H

#include <kernel/types.h>

/* System configuration set during installation */
typedef struct {
    char hostname[32];
    char username[32];
    int  theme;          /* 0=Catppuccin, 1=Nord, 2=Dracula, 3=Gruvbox */
    bool setup_complete;
} system_config_t;

/* Run the installation wizard (blocking). Returns when user finishes setup. */
void installer_run(void);

/* Get the system configuration after installation */
const system_config_t *installer_get_config(void);

#endif /* _ASTRA_KERNEL_INSTALLER_H */
