# Sprig-side receiver, run by Spryg (see reference/spryg/). Reads RGB565
# frames streamed over UART0 (J2 GP0/GP1) from firmware/esp32cam/esp32cam.ino
# and blits them into the middle of the screen.
#
# Protocol (must match esp32cam.ino exactly):
#   2 sync bytes 0xAA 0x55, then FRAME_W*FRAME_H*2 bytes of big-endian RGB565,
#   row-major, repeating forever. No length field -- frame size is fixed and
#   known by both ends.
#
# Sync strategy: the ESP32 dumps each frame as a fast burst. If we ever block
# on a fixed-size read (uart.read(256)) the RX ring buffer overflows mid-burst
# and drops bytes -- systematically eating the MAGIC marker so we never lock.
# So sync always DRAINS everything available (uart.read(uart.any())) to keep
# the buffer empty, and the bulk frame body is pulled straight into a fixed
# bytearray via readinto (no per-frame allocation, no fragmentation).

from machine import UART, Pin
import time

MAGIC = b"\xaa\x55"
FRAME_W = 160
FRAME_H = 120  # QQVGA from the camera; the Sprig screen is 160x128 landscape
FRAME_BYTES = FRAME_W * FRAME_H * 2
LINK_BAUD = 460800  # must match LINK_BAUD in esp32cam.ino

SCREEN_W = 160
SCREEN_ROW_BYTES = SCREEN_W * 2
Y_OFFSET = (128 - FRAME_H) // 2  # letterbox bars, top and bottom


def run(spryg):
    # UART0 defaults to tx=GP0, rx=GP1 on the RP2040, matching J2.
    # If your MicroPython build rejects `rxbuf`, just drop that kwarg.
    uart = UART(0, baudrate=LINK_BAUD, tx=Pin(0), rx=Pin(1), rxbuf=8192, timeout=1000)
    print("game.py: uart open, syncing...")

    frame = bytearray(FRAME_BYTES)
    mv = memoryview(frame)
    fc = 0

    leftover = _find_magic(uart, b"")
    print("game.py: locked onto stream")

    while True:
        # Seed the frame buffer with any bytes already read past the marker,
        # then pull the rest of the body directly into the buffer.
        n = len(leftover)
        if n >= FRAME_BYTES:
            mv[:] = leftover[:FRAME_BYTES]
            leftover = leftover[FRAME_BYTES:]
        else:
            if n:
                mv[:n] = leftover
            leftover = b""
            got = n
            while got < FRAME_BYTES:
                r = uart.readinto(mv[got:])
                if r:
                    got += r

        _blit(spryg, mv)
        spryg.flip()
        fc += 1
        print("frame", fc)

        # Frames are back-to-back, so the next 2 bytes should be the following
        # MAGIC. If they are, stay locked with zero search cost; otherwise a byte
        # slipped somewhere -- fall back to a full drain-and-search resync.
        hdr = _read_n(uart, len(MAGIC))
        if hdr != MAGIC:
            leftover = _find_magic(uart, hdr)


def _find_magic(uart, tail):
    """Drain all available bytes and hunt for MAGIC; return bytes read past it."""
    while True:
        n = uart.any()
        if not n:
            time.sleep_ms(2)
            continue
        chunk = uart.read(n)
        if not chunk:
            continue
        buf = tail + chunk if tail else chunk
        i = buf.find(MAGIC)
        if i != -1:
            return buf[i + len(MAGIC):]
        # keep only enough tail to catch a MAGIC split across the read boundary
        tail = buf[-(len(MAGIC) - 1):]


def _read_n(uart, n):
    """Blocking read of exactly n bytes."""
    out = uart.read(n)
    if out is None:
        out = b""
    while len(out) < n:
        more = uart.read(n - len(out))
        if more:
            out += more
    return out


def _blit(spryg, mv):
    for row in range(FRAME_H):
        dest = (row + Y_OFFSET) * SCREEN_ROW_BYTES
        src = row * SCREEN_ROW_BYTES
        spryg.buf[dest:dest + SCREEN_ROW_BYTES] = mv[src:src + SCREEN_ROW_BYTES]
