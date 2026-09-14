/*
 * Linker script MEMORY region for an application built to run from Slot A.
 * Splice into the normal STM32F407 app linker script in place of its
 * default FLASH region; everything else stays the same (.isr_vector, .text,
 * .data, .bss, heap/stack) - only the origin and length of FLASH change,
 * since the app now starts partway into flash instead of at 0x08000000.
 *
 * IMPORTANT: build two firmware binaries from the same source, one
 * linked against this script and one against STM32F407_app_slotB.ld.
 * The bootloader always writes into the *inactive* slot, so whichever
 * slot isn't currently running must be the one you send next.
 */

ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08010000, LENGTH = 448K  /* Slot A: sectors 4-7 */
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } > FLASH
  .text       : { . = ALIGN(4); *(.text*) *(.rodata*) . = ALIGN(4); _etext = .; } > FLASH

  _sidata = LOADADDR(.data);
  .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  .bss  : { . = ALIGN(4); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM

  ._user_heap_stack : { . = ALIGN(8); . = . + 0x400; . = ALIGN(8); } > RAM
}
