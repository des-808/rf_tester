/*
******************************************************************************
**
**  File        : LinkerScript.ld (QSPI XIP - Execute In Place)
**
**  Author      : STM32CubeIDE
**
**  Abstract    : Linker script for STM32H7 series with QSPI XIP
**                Bootloader in internal FLASH (32KB)
**                Main code and data in QSPI (16MB XIP)
**
**  Target      : STMicroelectronics STM32H750
**
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM_D1) + LENGTH(RAM_D1);
_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

/* Memory Areas */
MEMORY
{
  FLASH (rx)   : ORIGIN = 0x08000000, LENGTH = 32K    /* Bootloader */
  QSPI (rx)    : ORIGIN = 0x90000000, LENGTH = 8M    /* XIP Code */
  DTCMRAM (xrw): ORIGIN = 0x20000000, LENGTH = 128K   /* Data TCM */
  RAM_D1 (xrw) : ORIGIN = 0x24000000, LENGTH = 512K   /* AXI RAM */
  RAM_D2 (xrw) : ORIGIN = 0x30000000, LENGTH = 288K
  RAM_D3 (xrw) : ORIGIN = 0x38000000, LENGTH = 64K
  ITCMRAM (xrw): ORIGIN = 0x00000000, LENGTH = 64K
}

SECTIONS
{
  /* Bootloader vectors in internal FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* Code in QSPI */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >QSPI

  /* Read-only data in QSPI */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >QSPI

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >QSPI

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >QSPI

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >QSPI

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >QSPI

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >QSPI

  /* Load address (LMA) of .data section - location in QSPI */
  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM_D1 AT> QSPI

  /* Uninitialized data in RAM */
  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM_D1

  .ram_d1 (NOLOAD) :
  {
    . = ALIGN(4);
    *(.ram_d1)
    . = ALIGN(4);
  } >RAM_D1

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM_D1

  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}

