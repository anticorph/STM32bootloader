# STM32 UART Bootloader — VTOR Relocation, Firmware Validation, A/B Rollback

A bare-metal, dependency-free bootloader for STM32F4 (reference target:
STM32F407VG, 1 MB flash / 128 KB RAM) that:

- Receives new firmware over UART with a retry-protected, CRC32-checked
  transfer protocol.
- Stores firmware in one of two application slots (**A/B partitioning**)
  so an update never overwrites the currently-running image.
- Relocates the vector table (`SCB->VTOR`) and jumps into whichever
  slot is active.
- Validates a slot's firmware (CRC32 + sanity-checked vector table)
  before ever jumping to it.
- **Automatically rolls back** to the previous known-good image if the
  new firmware fails validation, never calls back in to confirm it's
  healthy, or crashes/watchdog-resets while on probation.

This is a reference implementation meant to be adapted, not a
drop-in-and-ship product - see "Hardening ideas" at the end.

## Memory layout (STM32F407VG, 1 MB flash)

| Region              | Address range             | Size   | Sectors |
|---------------------|----------------------------|--------|---------|
| Bootloader          | 0x08000000 - 0x08008000    | 32 KB  | 0–1     |
| Metadata log (A/B)  | 0x08008000 - 0x0800C000    | 16 KB  | 2       |
| Reserved            | 0x0800C000 - 0x08010000    | 16 KB  | 3       |
| **Slot A** (app)    | 0x08010000 - 0x08080000    | 448 KB | 4–7     |
| **Slot B** (app)    | 0x08080000 - 0x08100000    | 512 KB | 8–11    |

Change the constants in `Bootloader/Inc/flash_map.h` if you target a
different STM32 family. Note F1/L4/G4/G0/L0 parts use small uniform
pages instead of asymmetric sectors - `flash_driver.c`'s erase routine
is F4/F7-specific (`FLASH_CR_SNB`/`SER`) and needs a page-erase
equivalent on those parts.

## How the boot state machine works

Each app slot has a state stored in a small **ping-pong metadata log**
(`Bootloader/Src/boot_metadata.c`) written to its own flash sector, so
a power loss mid-write never corrupts the *previous* good record
(each record is magic + CRC32 checked on load; a torn write is simply
skipped and the last good record is used instead):

```
EMPTY → NEW → TESTING → VALID
                 │
                 └── (crash / no commit / CRC fail) → INVALID → rollback
```

1. **NEW** - firmware was just flashed, never booted.
2. **TESTING** - booted once; on probation. The bootloader sets a
   `PENDING` flag in a backup-domain register before jumping in.
3. **VALID** - the application called `boot_commit()` (see
   `Application/Inc/app_commit.h`), overwriting the flag with a
   `COMMIT` magic value. The bootloader sees this on the *next* boot
   and promotes the slot to VALID permanently.
4. **INVALID** - set when: CRC32 fails, the vector table looks bogus
   (stack pointer outside SRAM, reset vector outside the slot,
   missing Thumb bit), the app watchdog-reset while TESTING, the app
   never committed within `MAX_BOOT_ATTEMPTS` (default 3) boots, or a
   previous rollback already condemned it.

On every boot the bootloader validates the **active** slot first. If
that fails, it validates the **other** slot and switches to it if
valid (`main.c`'s rollback block) - that's the actual rollback. If
neither slot validates, the bootloader parks in an infinite loop
listening for a UART update instead of bricking the device.

### Why a backup register instead of just the flash state?

`boot_commit()` in the application only writes one word to
`RTC->BKP0R` - no flash unlock/erase/program code needs to be
duplicated into the application binary, and it's a single atomic
write, so there's no "torn commit" case to reason about.

## UART update protocol

Implemented in `Bootloader/Src/uart_protocol.c`, mirrored in
`Host/send_firmware.py`:

1. For 2 seconds after reset, the bootloader listens on USART2
   (PA2=TX, PA3=RX, 115200-8-N-1) for the ASCII string `BLUPDATE`. On
   match it replies `0x06` (ACK).
2. Host sends a 12-byte header: `size:u32-LE, crc32:u32-LE, reserved:u32-LE`.
   Bootloader replies ACK if `size` fits the inactive slot, else NAK.
3. Bootloader erases the inactive slot, then receives the image in
   ≤256-byte packets, each followed by a 4-byte CRC32 of just that
   packet. Bad packets get NAK'd and the host resends (up to 5 tries).
4. After the last packet, the bootloader recomputes the whole-image
   CRC32 and does a final ACK/NAK. On ACK, it marks the inactive slot
   `NEW`, makes it the active slot, and persists metadata — the new
   image boots (on probation) on the very next reset.

CRC32 is the standard zlib/PKZIP polynomial, so `Host/send_firmware.py`
uses Python's built-in `zlib.crc32()` with no extra glue.

If a transfer fails at any point, the previously active, already
-validated firmware is left completely untouched.

## Building

This repo intentionally does **not** include CMSIS device headers,
`startup_stm32f407xx.s`, or a full linker script - pull those from a
standard STM32CubeMX/CubeIDE project (or STM32CubeF4) for your part,
then:

1. Drop `Bootloader/Inc` and `Bootloader/Src` into a new CubeIDE
   project targeting your MCU as a firmware image of its own.
2. Replace that project's linker script `MEMORY` block with the one in
   `Bootloader/linker/STM32F407_bootloader.ld`.
3. Build/flash it once via SWD/JTAG (ST-Link) - this only has to
   happen once per board; all further updates go over UART.
4. For your actual application, build it **twice** - once linked
   against `Application/linker/STM32F407_app_slotA.ld` and once
   against `STM32F407_app_slotB.ld` - and add `app_commit.c`/`.h` to
   the app project. Call `boot_commit()` early in `main()` once you're
   confident the app is healthy, and feed an independent watchdog
   (IWDG) from the main loop after that point so a real hang gets
   caught and rolled back automatically.
5. At production time, flash Slot A's binary directly via SWD, and
   pre-populate one metadata record marking Slot A `VALID` with its
   real size/CRC (the current `main.c` leaves `size = 0` as a
   placeholder for this - set it from your production flashing
   script, or simply flash the metadata sector directly).
