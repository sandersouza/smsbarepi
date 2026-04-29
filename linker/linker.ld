ENTRY(_start)

SECTIONS
{
    . = 0x80000;

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
    }
    _etext = .;

    .rodata : {
        *(.rodata*)
    }
    PROVIDE(__init_start = .);
    PROVIDE(__init_end = .);

    .data : {
        *(.data*)
    }

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }
    _end = __bss_end;

    . = ALIGN(16);
    __stack_bottom = .;
    . += 0x100000; /* 1MiB stack for emulator bring-up (deep call paths/logging) */
    __stack_top = .;
}
