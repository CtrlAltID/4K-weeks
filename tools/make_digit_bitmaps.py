#!/usr/bin/env python3
"""Render the dial digits as antialiased bitmaps.

Pebble's font pipeline rasters TTFs to one bit per pixel, so text on the watch
has hard edges and there is no setting that changes it. The dial numbers
therefore bypass the font system: they are rendered here in greyscale and
blitted from bitmaps at runtime.

The display carries 2 bits per channel, so coverage is quantised to the four
levels it can show. The glyphs are written as white with an alpha ramp rather
than white pre-mixed against black, because the hand and the minute circle pass
under the numbers and pre-mixed grey would fringe them where they overlap.

Run from the watchface directory, with the SDK's own interpreter — freetype-py
lives there:

    ~/.local/share/pebble-sdk/SDKs/4.17/.venv/bin/python \
        tools/make_digit_bitmaps.py

It writes resources/images/digit_<ppem>_<index>.png and prints the metric
tables to paste into line_dash.c.
"""
import math
import os
import struct
import zlib

FONT = "resources/fonts/Jost-Medium.ttf"
OUT = "resources/images"
# The four hour cuts, giving digits 24 / 34 / 42 / 56 px tall.
PPEMS = (34, 48, 60, 80)
# The minute in the circle: the colon costs room, so the bare form gets the
# larger cut. Index 10 of the 25 set is the colon.
MINUTE_CUTS = {25: "0123456789:", 30: "0123456789"}

import freetype  # noqa: E402  (SDK venv only)


def render(ppem, chars="0123456789"):
    """Rasterise the run in 8-bit greyscale, return per-glyph coverage and metrics."""
    face = freetype.Face(FONT)
    face.set_pixel_sizes(0, ppem)
    # Jost's cap height is 0.7 em, so it falls on a pixel boundary only at
    # multiples of 10 ppem. At any other size the flat digits come out with a
    # half-covered top row against a solid baseline — 50 % against 100 % at
    # 25 ppem — and the digit reads as sitting low in its box. Squeezing the
    # outline vertically onto the grid, which is what a hinted font would do,
    # gives both edges back at a cost of at most 3 % of the height. The
    # transform is vertical only, so advance widths and side bearings are
    # unaffected.
    face.load_char("4", freetype.FT_LOAD_NO_SCALE | freetype.FT_LOAD_NO_HINTING)
    cap = face.glyph.outline.get_bbox().yMax * ppem / face.units_per_EM
    face.set_transform(
        freetype.Matrix(0x10000, 0, 0, int(round(cap) / cap * 0x10000)),
        freetype.Vector(0, 0),
    )
    out = {}
    for d, ch in enumerate(chars):
        face.load_char(ch, freetype.FT_LOAD_RENDER)  # greyscale, unhinted
        g = face.glyph
        b = g.bitmap
        cov = [[b.buffer[y * b.pitch + x] for x in range(b.width)]
               for y in range(b.rows)]
        out[d] = {
            "cov": cov,
            "w": b.width,
            "h": b.rows,
            "left": g.bitmap_left,
            "top": g.bitmap_top,          # rows above the baseline
            "adv": g.advance.x // 64,
        }
    return out


def quantise(v):
    """0-255 coverage to one of the four levels the display can show."""
    return round(v * 3 / 255)


# An hour number always faces one of twelve directions, one per hour mark, so
# its reach towards that mark can be measured here instead of estimated on the
# watch. Index k is the hour, the direction pointing outwards along its mark.
HOUR_DIRS = 12


def reach(m, cap, mid, k):
    """How far glyph `m` reaches towards hour mark `k`, from its own anchor.

    The anchor is the pen position on the middle of the optical band, which is
    where the drawing code puts a glyph. Measured against the ink rather than
    the bounding box: a "7" fills the top right of its box and leaves the bottom
    left empty, so a box would hold it a good two units off its mark at five
    o'clock. Pixels count from half coverage, where the eye reads the edge.

    Whole pixels are squares, so the far corner counts, not the near one — hence
    the two trailing terms, which cover the last half pixel in each axis.
    """
    a = math.radians(30.0 * k)
    ux, uy = math.sin(a), -math.cos(a)
    top = cap - m["top"] - mid          # first ink row, relative to the anchor
    best = None
    for y in range(m["h"]):
        for x in range(m["w"]):
            if quantise(m["cov"][y][x]) < 2:
                continue
            v = (m["left"] + x) * ux + (top + y) * uy
            if best is None or v > best:
                best = v
    return best + max(0.0, ux) + max(0.0, uy)


def optical_centre(m):
    """Ink mass of glyph `m`, and where its middle sits from the pen position.

    An hour number stands free beside its mark, and there the eye centres what
    it can see: a "1" carries almost all of its ink in the stem, right of the
    middle of its box, so centring the box leaves the stem beside the mark
    rather than on it. The minute does not get this — inside its ring the eye
    judges the air either side of the digits, which is a box relation, and
    centring the mass there measurably worsens it.
    """
    tot = 0
    sx = 0.0
    for y in range(m["h"]):
        for x in range(m["w"]):
            a = quantise(m["cov"][y][x])
            if a:
                tot += a
                sx += (m["left"] + x + 0.5) * a
    return tot, sx / tot


def write_png(path, w, h, rgba):
    raw = b"".join(b"\x00" + bytes(rgba[y]) for y in range(h))

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload +
                struct.pack(">I", zlib.crc32(tag + payload) & 0xffffffff))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def emit(ppem, chars="0123456789", prefix="s_b"):
    g = render(ppem, chars)
    n = len(chars)
    # Two different lines, and mixing them up costs a pixel.
    #
    # `hi` is the highest ink of the set, which the round digits reach because
    # they overshoot the cap. Every glyph is placed against it, so it is what
    # the top offsets are measured from. It comes off the digits alone: a colon
    # reaches nowhere near it, and letting it into the maximum would drop the
    # whole run by its own height.
    #
    # `capf` is the flat digits' cap height, which is what the eye reads as the
    # top of the number. The optical band runs from there to the baseline, so
    # its middle sits `hi - capf/2` below the top line — that, not `hi/2`, is
    # what the anchor has to land on.
    hi = max(g[d]["top"] for d in range(10))
    capf = g[4]["top"]        # "4" is flat-topped, and now grid-fitted
    # The band occupies rows [hi - capf, hi) below the top line, so its middle
    # row sits hi - (capf + 1) // 2 below it. Halving hi is a row out whenever
    # the cap height is odd.
    mid = hi - (capf + 1) // 2
    cap = hi
    for d, m in sorted(g.items()):
        rows = []
        for y in range(m["h"]):
            row = []
            for x in range(m["w"]):
                a = quantise(m["cov"][y][x]) * 85
                row += [255, 255, 255, a]
            rows.append(row)
        write_png(f"{OUT}/digit_{ppem}_{d}.png", m["w"], m["h"], rows)

    # The side bearings go signed: a "7" at the larger cuts hangs its flag past
    # the advance width, so the right bearing comes out negative.
    def table(kind, name, fn):
        return (f"static const {kind} {name}{ppem}[{n}] = {{"
                + ", ".join(str(fn(g[d])) for d in range(n)) + "};")

    print(f"// {ppem} ppem, digits {max(g[d]['h'] for d in range(10))} px tall"
          + (", index 10 is the colon" if n > 10 else ""))
    print(table("uint8_t", f"{prefix}_adv", lambda m: m["adv"]))
    print(table("int8_t", f"{prefix}_lsb", lambda m: m["left"]))
    print(table("int8_t", f"{prefix}_rsb", lambda m: m["adv"] - m["left"] - m["w"]))
    print(table("uint8_t", f"{prefix}_top", lambda m: cap - m["top"]))
    print(table("uint8_t", f"{prefix}_w", lambda m: m["w"]))
    print(table("uint8_t", f"{prefix}_h", lambda m: m["h"]))
    print(f"#define BMP_CAP_{ppem} {capf}  // flat cap height, the optical band")
    print(f"#define BMP_MID_{ppem} {mid}   // top line down to the middle of it")

    # Only the hours are placed against a mark; the minute sits in its circle.
    if prefix != "s_b":
        return
    oc = [optical_centre(g[d]) for d in range(n)]
    print("// Ink mass of each digit, and the middle of that mass measured from"
          " the pen\n// position in eighths of a pixel.")
    print(f"static const uint16_t {prefix}_mass{ppem}[{n}] = {{"
          + ", ".join(str(oc[d][0]) for d in range(n)) + "};")
    print(f"static const int16_t {prefix}_cen{ppem}[{n}] = {{"
          + ", ".join(str(int(round(oc[d][1] * 8))) for d in range(n)) + "};")
    print(f"// Reach towards the mark, by hour and then by digit, in pixels.")
    print(f"static const int8_t {prefix}_sup{ppem}[{HOUR_DIRS}][{n}] = {{")
    for k in range(HOUR_DIRS):
        row = [int(round(reach(g[d], cap, mid, k))) for d in range(n)]
        print("  {" + ", ".join(f"{v:4d}" for v in row) + f"}},  // {k or 12} o'clock")
    print("};")


def main():
    os.makedirs(OUT, exist_ok=True)
    print("// Generated by tools/make_digit_bitmaps.py — do not edit by hand.")
    for ppem in PPEMS:
        emit(ppem)
    for ppem, chars in MINUTE_CUTS.items():
        emit(ppem, chars, "s_m")


if __name__ == "__main__":
    main()
