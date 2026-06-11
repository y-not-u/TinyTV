#!/usr/bin/env python3
"""Generate a GB2312 16x16 Chinese bitmap font for TFT_eSPI on ESP8266.

Outputs firmware/SmallDesktopDisplay/ChineseFont.h (PROGMEM data).

Font format:
  - chineseFontLookup[]: sorted by Unicode, 4 bytes per entry (unicode + index)
  - chineseFontBitmaps[]: 32 bytes per glyph (16 rows × 2 bytes, MSB-first)
"""

import struct, sys, os
from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "/System/Library/Fonts/STHeiti Light.ttc"
FONT_SIZE = 16
GLYPH_W = 16
GLYPH_H = 16

def get_font():
    for p in [FONT_PATH, "/System/Library/Fonts/STHeiti Medium.ttc",
              "/System/Library/Fonts/Supplemental/Songti.ttc",
              "/Library/Fonts/Arial Unicode.ttf"]:
        try:
            return ImageFont.truetype(p, FONT_SIZE, encoding='unic')
        except:
            pass
    raise RuntimeError("No CJK font found")

def render_glyph(font, unicode_cp):
    """Render a single Unicode char to 16x16 1-bit bitmap. Returns 32 bytes or None."""
    img = Image.new('L', (GLYPH_W, GLYPH_H), 0)
    draw = ImageDraw.Draw(img)
    try:
        bbox = draw.textbbox((0, 0), chr(unicode_cp), font=font)
        if bbox[2] <= bbox[0] or bbox[3] <= bbox[1]:
            return None
        cw, ch = bbox[2] - bbox[0], bbox[3] - bbox[1]
        ox = (GLYPH_W - cw) // 2 - bbox[0]
        oy = (GLYPH_H - ch) // 2 - bbox[1]
        draw.text((ox, oy), chr(unicode_cp), font=font, fill=255)
        pixels = img.load()
        out = bytearray()
        for y in range(GLYPH_H):
            row = 0
            for x in range(GLYPH_W):
                if pixels[x, y] > 128:
                    row |= (1 << (15 - x))
            out.append((row >> 8) & 0xFF)
            out.append(row & 0xFF)
        return bytes(out)
    except:
        return None

def gb2312_unicodes(font):
    """Generate GB2312-80 character set.
    Characters are arranged in 94 rows × 94 columns, areas 16-87.
    Level 1 (most common): areas 16-55, 3755 chars
    Level 2: areas 56-87, 3008 chars
    Total: 6763 chars
    """
    chars = []
    # GB2312 EUC-CN byte pairs range from 0xA1A1 to 0xFEFE
    # Area (qh) 16-55 = high byte 0xB0-0xD7
    # Area (qh) 56-87 = high byte 0xD8-0xF7
    for qh in range(16, 88):
        for wh in range(1, 95):
            gb_high = qh + 0xA0
            gb_low = wh + 0xA0
            # Convert EUC-CN to Unicode: decode using 'gb2312' codec
            try:
                cp = bytes([gb_high, gb_low]).decode('gb2312')
                unicode_cp = ord(cp)
                glyph = render_glyph(font, unicode_cp)
                if glyph:
                    chars.append((unicode_cp, glyph))
            except:
                pass
        if qh % 10 == 0:
            print(f"  Area {qh}/87: {len(chars)} chars", file=sys.stderr)
    return chars

def main():
    print("Loading font...", file=sys.stderr)
    font = get_font()
    print("Scanning GB2312 characters...", file=sys.stderr)
    chars = gb2312_unicodes(font)
    chars.sort(key=lambda x: x[0])
    print(f"Total: {len(chars)} characters", file=sys.stderr)

    out_dir = "/Users/apple/Workspace/01-Temp/TinyTV/firmware/SmallDesktopDisplay"
    h_path = f"{out_dir}/ChineseFont.h"

    with open(h_path, 'w') as f:
        f.write(f"// Auto-generated 16x16 Chinese bitmap font (GB2312)\n")
        f.write(f"// Generated from STHeiti Light, {len(chars)} chars\n")
        f.write(f"#ifndef CHINESE_FONT_H\n#define CHINESE_FONT_H\n\n")
        f.write(f'#include <pgmspace.h>\n\n')
        f.write(f"#define CHINESE_FONT_CHAR_COUNT {len(chars)}\n")
        f.write(f"#define CHINESE_FONT_W 16\n")
        f.write(f"#define CHINESE_FONT_H 16\n\n")

        # Lookup table: pairs of {uint16_t unicode, uint16_t index}
        f.write("// Lookup table: unicode, glyph_index (sorted by unicode)\n")
        f.write("const uint16_t chineseFontLookup[] PROGMEM = {\n")
        for i, (cp, _) in enumerate(chars):
            f.write(f"  0x{cp:04X}, {i},\n")
        f.write("};\n\n")

        # Bitmap data
        total_bits = 0
        f.write("// Glyph bitmaps: 32 bytes per char (16 rows × 2 bytes)\n")
        f.write("const uint8_t chineseFontBitmaps[] PROGMEM = {\n")
        for i, (cp, data) in enumerate(chars):
            if i > 0:
                f.write("\n")
            hex_str = ", ".join(f"0x{b:02X}" for b in data)
            f.write(f"  // U+{cp:04X} ({chr(cp)})\n")
            f.write(f"  {hex_str},\n")
            total_bits += sum(bin(b).count('1') for b in data)  # rough density
        f.write("};\n\n#endif\n")

    bitmap_size = len(chars) * 32
    lookup_size = len(chars) * 4
    total = bitmap_size + lookup_size
    print(f"\n  Bitmaps: {bitmap_size} bytes", file=sys.stderr)
    print(f"  Lookup:  {lookup_size} bytes", file=sys.stderr)
    print(f"  Total:   {total} bytes ({total/1024:.1f} KB)", file=sys.stderr)

if __name__ == '__main__':
    main()
