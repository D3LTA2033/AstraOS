/* User program linker script — flat binary at 0x00400000 */
ENTRY(_start)
SECTIONS
{
    . = 0x00400000;
    .text : {
        *entry*.o(.text*)
        *(.text*)
    }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    _user_end = .;
}
