"""Draw the app icon (4x4 glowing lattice nodes + focus ring) as a PNG, stdlib only."""
import math
import struct
import sys
import zlib

S = 512
BG, AMBER, OFF = (11, 13, 16), (232, 163, 61), (42, 47, 55)


def png(path, px):
    raw = b"".join(b"\x00" + bytes(px[y * S * 4:(y + 1) * S * 4]) for y in range(S))
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", S, S, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(data)


def main(path):
    px = bytearray(S * S * 4)
    cell = S * 0.8 / 4
    x0 = S * 0.1
    nodes = [(x0 + (i + 0.5) * cell, x0 + (j + 0.5) * cell) for j in range(4) for i in range(4)]
    fx, fy, fr = x0 + 1.5 * cell, x0 + 1.6 * cell, 1.3 * cell
    corner = S * 0.18
    for y in range(S):
        for x in range(S):
            # rounded square mask
            dx = max(abs(x + 0.5 - S / 2) - (S / 2 - corner), 0)
            dy = max(abs(y + 0.5 - S / 2) - (S / 2 - corner), 0)
            a = max(0.0, min(1.0, corner - math.hypot(dx, dy)))
            r, g, b = BG
            for (nx, ny) in nodes:
                d = math.hypot(x - nx, y - ny)
                e = math.exp(-((nx - fx) ** 2 + (ny - fy) ** 2) / (2 * fr * fr))
                glow = e * math.exp(-(d / (cell * 0.32)) ** 2) * 0.9
                core = max(0.0, min(1.0, cell * 0.11 - d + 0.5))
                col = [OFF[k] + (AMBER[k] - OFF[k]) * (0.2 + 0.8 * e) for k in range(3)]
                r, g, b = [c + (AMBER[k] - c) * glow * 0.55 for k, c in enumerate((r, g, b))]
                r, g, b = [c + (col[k] - c) * core for k, c in enumerate((r, g, b))]
            ring = max(0.0, 1.0 - abs(math.hypot(x - fx, y - fy) - fr * 0.55) / 2.5)
            r, g, b = [c + (216 - c) * ring * 0.8 for c in (r, g, b)]
            i = (y * S + x) * 4
            px[i:i + 4] = bytes((int(r), int(g), int(b), int(a * 255)))
    png(path, px)


if __name__ == "__main__":
    main(sys.argv[1])
