"""Generate a dense UTF-8 glyph atlas for DebugText from PixelMplus."""

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("font", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--columns", type=int, default=128)
    parser.add_argument("--base-size", type=int, default=12)
    parser.add_argument("--scale", type=int, default=2)
    return parser.parse_args()


def read_unicode_codepoints(font_path: Path) -> set[int]:
    """Read Unicode cmap format 4/12 without requiring fontTools."""
    data = font_path.read_bytes()
    num_tables = struct.unpack_from(">H", data, 4)[0]
    cmap_offset = None
    for table_index in range(num_tables):
        record_offset = 12 + table_index * 16
        tag, _checksum, offset, _length = struct.unpack_from(">4sIII", data, record_offset)
        if tag == b"cmap":
            cmap_offset = offset
            break
    if cmap_offset is None:
        raise ValueError("font has no cmap table")

    subtable_count = struct.unpack_from(">H", data, cmap_offset + 2)[0]
    subtables: list[int] = []
    for record_index in range(subtable_count):
        record_offset = cmap_offset + 4 + record_index * 8
        platform_id, _encoding_id, relative_offset = struct.unpack_from(">HHI", data, record_offset)
        if platform_id in (0, 3):
            subtables.append(cmap_offset + relative_offset)

    codepoints: set[int] = set()
    for subtable_offset in subtables:
        cmap_format = struct.unpack_from(">H", data, subtable_offset)[0]
        if cmap_format == 12:
            group_count = struct.unpack_from(">I", data, subtable_offset + 12)[0]
            for group_index in range(group_count):
                start, end, start_glyph = struct.unpack_from(
                    ">III", data, subtable_offset + 16 + group_index * 12
                )
                if start_glyph != 0:
                    codepoints.update(range(start, min(end, 0xFFFF) + 1))
        elif cmap_format == 4:
            segment_count = struct.unpack_from(">H", data, subtable_offset + 6)[0] // 2
            end_codes_offset = subtable_offset + 14
            start_codes_offset = end_codes_offset + segment_count * 2 + 2
            deltas_offset = start_codes_offset + segment_count * 2
            range_offsets_offset = deltas_offset + segment_count * 2
            for segment_index in range(segment_count):
                end = struct.unpack_from(">H", data, end_codes_offset + segment_index * 2)[0]
                start = struct.unpack_from(">H", data, start_codes_offset + segment_index * 2)[0]
                delta = struct.unpack_from(">h", data, deltas_offset + segment_index * 2)[0]
                range_offset_address = range_offsets_offset + segment_index * 2
                range_offset = struct.unpack_from(">H", data, range_offset_address)[0]
                for codepoint in range(start, end + 1):
                    if codepoint == 0xFFFF:
                        continue
                    if range_offset == 0:
                        glyph_id = (codepoint + delta) & 0xFFFF
                    else:
                        glyph_address = range_offset_address + range_offset + (codepoint - start) * 2
                        glyph_id = struct.unpack_from(">H", data, glyph_address)[0]
                        if glyph_id:
                            glyph_id = (glyph_id + delta) & 0xFFFF
                    if glyph_id:
                        codepoints.add(codepoint)
    return codepoints


def main() -> None:
    args = parse_args()
    codepoints = {
        codepoint
        for codepoint in read_unicode_codepoints(args.font)
        if 0x20 <= codepoint <= 0xFFFF
    }
    codepoints.update(range(0x20, 0x7F))
    ordered = [0x20] + sorted(codepoints - {0x20})

    columns = args.columns
    rows = math.ceil(len(ordered) / columns)
    base_cell_width = args.base_size
    base_cell_height = 16
    cell_width = base_cell_width * args.scale
    cell_height = base_cell_height * args.scale

    font = ImageFont.truetype(str(args.font), args.base_size)
    atlas = Image.new(
        "RGBA",
        (columns * cell_width, rows * cell_height),
        (255, 255, 255, 0),
    )

    for index, codepoint in enumerate(ordered):
        if codepoint == 0x20:
            continue
        glyph = Image.new("L", (base_cell_width, base_cell_height), 0)
        draw = ImageDraw.Draw(glyph)
        character = chr(codepoint)
        bbox = draw.textbbox((0, 0), character, font=font)
        glyph_width = bbox[2] - bbox[0]
        x = (base_cell_width - glyph_width) // 2 - bbox[0]
        y = -bbox[1]
        draw.text((x, y), character, font=font, fill=255)
        glyph = glyph.point(lambda value: 255 if value >= 128 else 0)
        glyph = glyph.resize((cell_width, cell_height), Image.Resampling.NEAREST)
        alpha = Image.new("RGBA", glyph.size, (255, 255, 255, 0))
        alpha.putalpha(glyph)
        atlas.alpha_composite(
            alpha,
            ((index % columns) * cell_width, (index // columns) * cell_height),
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(args.output, optimize=True)
    map_path = args.output.with_suffix(args.output.suffix + ".glyphs")
    with map_path.open("w", encoding="ascii", newline="\n") as glyph_map:
        glyph_map.write(f"{columns} {rows}\n")
        glyph_map.writelines(f"{codepoint:04X}\n" for codepoint in ordered)

    print(
        f"generated {len(ordered)} glyphs: "
        f"{atlas.width}x{atlas.height}, {columns}x{rows} cells"
    )


if __name__ == "__main__":
    main()
