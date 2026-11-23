#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generates app.ico (a 256x256 PNG-compressed icon) without external tools."""

import os
import struct
import zlib

SIZE = 256
OUTPUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'app.ico')
SS = 4  # supersampling factor

BG_TOP = (43, 108, 240, 255)
BG_BOT = (26, 79, 214, 255)
PAGE = (255, 255, 255, 255)
FOLD = (208, 215, 222, 255)
LINE = (140, 149, 159, 255)
ARROW = (43, 108, 240, 255)


def lerp(a, b, t):
    return a + (b - a) * t


def inside_rounded(x, y, x0, y0, x1, y1, r):
    if x < x0 or x > x1 or y < y0 or y > y1:
        return False
    cx = min(max(x, x0 + r), x1 - r)
    cy = min(max(y, y0 + r), y1 - r)
    dx = x - cx
    dy = y - cy
    return dx * dx + dy * dy <= r * r


def point_in_poly(x, y, pts):
    inside = False
    n = len(pts)
    j = n - 1
    for i in range(n):
        xi, yi = pts[i]
        xj, yj = pts[j]
        if (yi > y) != (yj > y):
            xint = (xj - xi) * (y - yi) / (yj - yi) + xi
            if x < xint:
                inside = not inside
        j = i
    return inside


def sample(x, y):
    if not inside_rounded(x, y, 8, 8, 248, 248, 48):
        return (0, 0, 0, 0)

    t = (y - 8) / 240.0
    color = tuple(int(lerp(BG_TOP[i], BG_BOT[i], t)) for i in range(4))

    if inside_rounded(x, y, 62, 44, 194, 212, 12):
        color = PAGE

    if point_in_poly(x, y, [(162, 44), (194, 44), (194, 76)]):
        color = FOLD

    for ly in (96, 122, 148):
        if inside_rounded(x, y, 82, ly, 174, ly + 10, 5):
            color = LINE

    if inside_rounded(x, y, 118, 158, 138, 190, 4):
        color = ARROW
    if point_in_poly(x, y, [(104, 188), (152, 188), (128, 210)]):
        color = ARROW

    return color


def png_chunk(tag, data):
    return (struct.pack('>I', len(data)) + tag + data +
            struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF))


def make_png():
    rows = []
    for oy in range(SIZE):
        row = bytearray()
        for ox in range(SIZE):
            r = g = b = a = 0
            for sy in range(SS):
                for sx in range(SS):
                    px = ox + (sx + 0.5) / SS
                    py = oy + (sy + 0.5) / SS
                    c = sample(px, py)
                    r += c[0]
                    g += c[1]
                    b += c[2]
                    a += c[3]
            n = SS * SS
            row += bytes((r // n, g // n, b // n, a // n))
        rows.append(bytes(row))

    raw = b''.join(b'\x00' + row for row in rows)
    ihdr = struct.pack('>IIBBBBB', SIZE, SIZE, 8, 6, 0, 0, 0)
    return (b'\x89PNG\r\n\x1a\n' +
            png_chunk(b'IHDR', ihdr) +
            png_chunk(b'IDAT', zlib.compress(raw, 9)) +
            png_chunk(b'IEND', b''))


def main():
    png = make_png()
    header = struct.pack('<HHH', 0, 1, 1)  # reserved, type=1 (icon), count=1
    entry = struct.pack('<BBBBHHII', 0, 0, 0, 0, 1, 32, len(png), 22)
    with open(OUTPUT, 'wb') as f:
        f.write(header + entry + png)
    print('Wrote %s (%d bytes)' % (OUTPUT, len(header) + len(entry) + len(png)))


if __name__ == '__main__':
    main()
