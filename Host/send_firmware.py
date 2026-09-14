#!/usr/bin/env python3
"""Host-side companion for the STM32 UART bootloader.

Protocol (must match Bootloader/Src/uart_protocol.c exactly):
  1. Host repeatedly sends the ASCII magic "BLUPDATE" until the device
     replies with 0x06 (ACK) - this only works during the device's
     post-reset update window (default 2 seconds).
  2. Host sends a 12-byte header: size:u32-LE, crc32:u32-LE, reserved:u32-LE.
     Device replies ACK/NAK.
  3. Host sends the image in up to 256-byte packets, each followed by
     a 4-byte CRC32 (LE) of just that packet's payload. Device replies
     ACK/NAK per packet; NAK means "resend this same packet".
  4. After the last packet, device recomputes the whole-image CRC32
     and sends a final ACK/NAK.

Requires: pip install pyserial

Usage:
    python3 send_firmware.py /dev/ttyUSB0 firmware.bin --baud 115200
"""
import argparse
import struct
import sys
import time
import zlib

import serial

MAGIC = b"BLUPDATE"
ACK = 0x06
NAK = 0x15
PKT_MAX_PAYLOAD = 256
MAX_RETRIES = 5


def read_byte(ser, context=""):
    b = ser.read(1)
    if not b:
        raise TimeoutError(f"no response from bootloader {context}".strip())
    return b[0]


def send_update_request(ser, window_s=10):
    print("Waiting for bootloader update window (reset the board now)...")
    deadline = time.time() + window_s
    while time.time() < deadline:
        ser.write(MAGIC)
        ser.flush()
        b = ser.read(1)
        if b and b[0] == ACK:
            print("Bootloader acknowledged - starting transfer.")
            return
    raise TimeoutError("bootloader never acknowledged the update request")


def send_firmware(port, path, baud):
    with open(path, "rb") as f:
        data = f.read()

    size = len(data)
    crc = zlib.crc32(data) & 0xFFFFFFFF
    print(f"Image: {path} ({size} bytes, crc32=0x{crc:08X})")

    with serial.Serial(port, baud, timeout=0.5) as ser:
        send_update_request(ser)

        ser.write(struct.pack("<III", size, crc, 0))
        if read_byte(ser, "after header") != ACK:
            raise RuntimeError("bootloader rejected header (image too large for slot?)")

        sent = 0
        while sent < size:
            chunk = data[sent:sent + PKT_MAX_PAYLOAD]
            pkt_crc = zlib.crc32(chunk) & 0xFFFFFFFF

            for attempt in range(1, MAX_RETRIES + 1):
                ser.write(chunk)
                ser.write(struct.pack("<I", pkt_crc))
                resp = read_byte(ser, f"for packet at offset {sent}")
                if resp == ACK:
                    break
                print(f"  packet at {sent} NAK'd, retry {attempt}/{MAX_RETRIES}")
            else:
                raise RuntimeError(f"packet at offset {sent} failed after {MAX_RETRIES} retries")

            sent += len(chunk)
            print(f"\r  {sent}/{size} bytes ({100 * sent // size}%)", end="", flush=True)

        print()
        if read_byte(ser, "final status") != ACK:
            raise RuntimeError("device reported a CRC mismatch after the full transfer")

        print("Firmware accepted. Device will boot it on probation and self-test it.")
        print("If the new firmware never calls boot_commit(), the bootloader will")
        print("automatically roll back to the previous image on the next reset.")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", help="Serial port, e.g. /dev/ttyUSB0 or COM5")
    ap.add_argument("firmware", help="Path to the raw .bin firmware image")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    try:
        send_firmware(args.port, args.firmware, args.baud)
    except Exception as e:
        print(f"\nUpdate failed: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
