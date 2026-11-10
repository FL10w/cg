"""Recreate the bundled BMP with the standard library; not used at runtime."""
from pathlib import Path
import struct


def main():
    width, height, tile = 512, 256, 32
    palette = [(38, 133, 179), (232, 174, 83), (207, 217, 218), (48, 74, 107)]
    pixels = bytearray()
    # BMP rows are stored bottom-up, BGR, padded to a multiple of four bytes.
    for y in reversed(range(height)):
        for x in range(width):
            color = palette[((x // tile) + (y // tile)) % len(palette)]
            if x % tile < 2 or y % tile < 2:
                color = (32, 43, 60)
            pixels.extend(reversed(color))
    header = struct.pack('<2sIHHI', b'BM', 54 + len(pixels), 0, 0, 54)
    header += struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, len(pixels), 2835, 2835, 0, 0)
    path = Path(__file__).resolve().parents[1] / 'assets' / 'torus.bmp'
    path.write_bytes(header + pixels)
    print(f'{path.name}: {width}x{height}, {len(header) + len(pixels)} bytes')


if __name__ == '__main__':
    main()
