/* Linker script for NXP S32K144 (512KB Flash, 64KB SRAM) */

ENTRY(Reset_Handler)

MEMORY
{
    /* Flash memory: 512KB */
    m_interrupts          (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00000400
    m_flash_config        (rx)  : ORIGIN = 0x00000400, LENGTH = 0x00000010
    m_text                (rx)  : ORIGIN = 0x00000410, LENGTH = 0x0007FBF0

    /* SRAM memory: 64KB (SRAM_L: 32KB + SRAM_U: 32KB) */
    m_data                (rwx) : ORIGIN = 0x1FFF8000, LENGTH = 0x00010000
}

__stack_size = 0x1000;
__heap_size  = 0x0800;

SECTIONS
{
    /* Interrupt vector table in Flash */
    .interrupts :
    {
        __VECTOR_TABLE = .;
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > m_interrupts

    /* Flash Configuration Field (Backdoor Comparison Key, Flash Security, FOPT) */
    .flash_config :
    {
        KEEP(*(.flash_config))
        . = ALIGN(4);
    } > m_flash_config

    /* Program code and constants */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP (*(.init))
        KEEP (*(.fini))
        . = ALIGN(4);
    } > m_text

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > m_text

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > m_text

    .ctors :
    {
        __CTOR_LIST__ = .;
        KEEP (*crtbegin.o(.ctors))
        KEEP (*crtbegin?.o(.ctors))
        KEEP (*(EXCLUDE_FILE(*crtend.o *crtend?.o) .ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
        __CTOR_END__ = .;
    } > m_text

    .dtors :
    {
        __DTOR_LIST__ = .;
        KEEP (*crtbegin.o(.dtors))
        KEEP (*crtbegin?.o(.dtors))
        KEEP (*(EXCLUDE_FILE(*crtend.o *crtend?.o) .dtors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*(.dtors))
        __DTOR_END__ = .;
    } > m_text

    /* Initialized data section (stored in Flash, copied to RAM at startup) */
    _etext = LOADADDR(.data);

    .data : AT(_etext)
    {
        . = ALIGN(4);
        __data_start__ = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        __data_end__ = .;
    } > m_data

    /* Uninitialized data section (BSS, zero-initialized at startup) */
    .bss :
    {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > m_data

    /* Heap section */
    .heap :
    {
        . = ALIGN(8);
        __heap_start__ = .;
        . = . + __heap_size;
        __heap_end__ = .;
    } > m_data

    /* Stack section (placed at the top of RAM) */
    .stack :
    {
        . = ALIGN(8);
        . = . + __stack_size;
        __stack_top = .;
        __StackTop = .;
    } > m_data
}
