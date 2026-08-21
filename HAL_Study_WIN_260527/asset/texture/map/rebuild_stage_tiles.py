from pathlib import Path

from PIL import Image, ImageFilter, ImageStat

from rebuild_structure_tiles import (
    EAST,
    NORTH,
    NORTH_EAST,
    NORTH_WEST,
    SOUTH,
    SOUTH_EAST,
    SOUTH_WEST,
    WEST,
    classify_structure,
)


ROOT = Path(__file__).resolve().parent
TILE_SIZE = 60
PREVIEW = ROOT / "stage_ground_style_preview.png"

STAGES = {
    "ice": {
        "source": ROOT / "ice_ground_atlas_source.png",
        "output": ROOT / "stage" / "ice",
        "names": (
            "ice_plain.png",
            "ice_frost.png",
            "ice_cracked.png",
            "ice_path.png",
        ),
    },
    "fire": {
        "source": ROOT / "fire_ground_atlas_source.png",
        "output": ROOT / "stage" / "fire",
        "names": (
            "basalt_plain.png",
            "basalt_embers.png",
            "basalt_cracked.png",
            "basalt_path.png",
        ),
    },
}

STRUCTURE_NAMES = (
    "void_deep.png",
    "void_mottle.png",
    "cliff_edge.png",
    "cliff_edge_moss.png",
    "cliff_inner_corner.png",
    "cliff_outer_corner.png",
    "cliff_channel.png",
    "cliff_cap.png",
    "cliff_pillar.png",
)

STRUCTURE_PALETTES = {
    "ice": (
        (3, 11, 27),
        (18, 42, 66),
        (53, 96, 120),
        (128, 181, 195),
        (204, 236, 238),
    ),
    "fire": (
        (18, 6, 10),
        (43, 22, 25),
        (78, 45, 39),
        (142, 77, 48),
        (226, 109, 42),
    ),
}


def match_average_color(image: Image.Image, reference: Image.Image) -> Image.Image:
    source_mean = ImageStat.Stat(image.convert("RGB")).mean
    target_mean = ImageStat.Stat(reference.convert("RGB")).mean
    channels = []
    for channel, source, target in zip(
        image.convert("RGB").split(), source_mean, target_mean
    ):
        offset = target - source
        channels.append(
            channel.point(
                lambda value, delta=offset: max(0, min(255, round(value + delta)))
            )
        )
    return Image.merge("RGB", channels)


def make_edges_periodic(image: Image.Image, blend_width: int = 5) -> Image.Image:
    """Blend toward identical opposite edges so point-sampled repetition has no seam."""
    image = image.convert("RGB")
    pixels = image.load()
    width, height = image.size

    for offset in range(blend_width):
        strength = (blend_width - offset) / blend_width
        left_x = offset
        right_x = width - 1 - offset
        for y in range(height):
            left = pixels[left_x, y]
            right = pixels[right_x, y]
            average = tuple((left[c] + right[c]) // 2 for c in range(3))
            pixels[left_x, y] = tuple(
                round(left[c] * (1.0 - strength) + average[c] * strength)
                for c in range(3)
            )
            pixels[right_x, y] = tuple(
                round(right[c] * (1.0 - strength) + average[c] * strength)
                for c in range(3)
            )

    for offset in range(blend_width):
        strength = (blend_width - offset) / blend_width
        top_y = offset
        bottom_y = height - 1 - offset
        for x in range(width):
            top = pixels[x, top_y]
            bottom = pixels[x, bottom_y]
            average = tuple((top[c] + bottom[c]) // 2 for c in range(3))
            pixels[x, top_y] = tuple(
                round(top[c] * (1.0 - strength) + average[c] * strength)
                for c in range(3)
            )
            pixels[x, bottom_y] = tuple(
                round(bottom[c] * (1.0 - strength) + average[c] * strength)
                for c in range(3)
            )

    return image


def force_exact_outer_edges(image: Image.Image) -> Image.Image:
    """Guarantee exact L/R and T/B pixels after palette reduction."""
    image = image.convert("RGB")
    pixels = image.load()
    width, height = image.size

    for y in range(height):
        average = tuple(
            (pixels[0, y][c] + pixels[width - 1, y][c]) // 2 for c in range(3)
        )
        pixels[0, y] = average
        pixels[width - 1, y] = average

    for x in range(width):
        average = tuple(
            (pixels[x, 0][c] + pixels[x, height - 1][c]) // 2 for c in range(3)
        )
        pixels[x, 0] = average
        pixels[x, height - 1] = average

    return image


def prepare_tile(tile: Image.Image) -> Image.Image:
    tile = tile.convert("RGB").resize(
        (TILE_SIZE, TILE_SIZE), Image.Resampling.LANCZOS
    )
    # Preserve the generated material shapes while removing busy brush noise.
    softened = tile.filter(ImageFilter.GaussianBlur(radius=1.05))
    tile = Image.blend(tile, softened, 0.45)
    tile = make_edges_periodic(tile)
    tile = tile.quantize(
        colors=24,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    ).convert("RGB")
    return force_exact_outer_edges(tile)


def build_stage(stage: dict[str, object]) -> None:
    source_path = stage["source"]
    output_dir = stage["output"]
    names = stage["names"]
    assert isinstance(source_path, Path)
    assert isinstance(output_dir, Path)
    assert isinstance(names, tuple)

    output_dir.mkdir(parents=True, exist_ok=True)
    with Image.open(source_path) as source:
        atlas = source.convert("RGB")
        width, height = atlas.size
        if width != height:
            raise ValueError(f"Stage atlas must be square, got {width}x{height}")

        tiles = []
        for index, name in enumerate(names):
            row, column = divmod(index, 2)
            left = column * width // 2
            right = (column + 1) * width // 2
            top = row * height // 2
            bottom = (row + 1) * height // 2
            tile = prepare_tile(atlas.crop((left, top, right, bottom)))
            tiles.append((name, tile))

    # Variant markings should read without creating a checkerboard of brightness.
    base = tiles[0][1]
    normalized = [tiles[0]]
    for name, tile in tiles[1:3]:
        tile = match_average_color(tile, base)
        tile = make_edges_periodic(tile)
        tile = tile.quantize(
            colors=24,
            method=Image.Quantize.MEDIANCUT,
            dither=Image.Dither.NONE,
        ).convert("RGB")
        normalized.append((name, force_exact_outer_edges(tile)))
    normalized.append(tiles[3])

    for name, tile in normalized:
        tile.save(output_dir / name, format="PNG", optimize=True)


def sample_palette(
    palette: tuple[tuple[int, int, int], ...], amount: float
) -> tuple[int, int, int]:
    amount = max(0.0, min(1.0, amount))
    position = amount * (len(palette) - 1)
    index = min(int(position), len(palette) - 2)
    blend = position - index
    return tuple(
        round(palette[index][channel] * (1.0 - blend) +
              palette[index + 1][channel] * blend)
        for channel in range(3)
    )


def recolor_structure(image: Image.Image, stage_name: str) -> Image.Image:
    """Map common source luminance to a biome palette without breaking tile joins."""
    source = image.convert("RGB")
    palette = STRUCTURE_PALETTES[stage_name]
    result = Image.new("RGB", source.size)
    source_pixels = source.load()
    result_pixels = result.load()

    for y in range(source.height):
        for x in range(source.width):
            red, green, blue = source_pixels[x, y]
            luminance = red * 0.2126 + green * 0.7152 + blue * 0.0722
            # Runtime cliff assets live mostly in the 10..120 luminance range.
            amount = (luminance - 10.0) / 110.0
            result_pixels[x, y] = sample_palette(palette, amount)
    return result


def build_stage_structures() -> None:
    source_dir = ROOT / "structure"
    for stage_name, stage in STAGES.items():
        output_root = stage["output"]
        assert isinstance(output_root, Path)
        output_dir = output_root / "structure"
        output_dir.mkdir(parents=True, exist_ok=True)
        for name in STRUCTURE_NAMES:
            with Image.open(source_dir / name) as source:
                recolored = recolor_structure(source, stage_name)
            recolored.save(output_dir / name, format="PNG", optimize=True)


def build_preview() -> None:
    panel_width = 8 * TILE_SIZE
    panel_height = 6 * TILE_SIZE
    preview = Image.new("RGB", (panel_width * 2, panel_height))

    for panel, (stage_name, stage) in enumerate(STAGES.items()):
        output_dir = stage["output"]
        names = stage["names"]
        assert isinstance(output_dir, Path)
        assert isinstance(names, tuple)
        tiles = {
            Path(name).stem: Image.open(output_dir / name).convert("RGB")
            for name in names
        }
        prefix = "ice" if stage_name == "ice" else "basalt"

        for y in range(6):
            for x in range(8):
                tile_name = f"{prefix}_plain"
                if x in (3, 4):
                    tile_name = f"{prefix}_path"
                elif (x, y) in {(1, 1), (6, 4)}:
                    tile_name = (
                        "ice_frost" if stage_name == "ice" else "basalt_embers"
                    )
                elif (x, y) in {(1, 4), (6, 1)}:
                    tile_name = f"{prefix}_cracked"
                preview.paste(
                    tiles[tile_name],
                    (panel * panel_width + x * TILE_SIZE, y * TILE_SIZE),
                )

    preview.save(PREVIEW, format="PNG", optimize=True)


def build_dungeon_preview(stage_name: str, stage: dict[str, object]) -> None:
    output_root = stage["output"]
    names = stage["names"]
    assert isinstance(output_root, Path)
    assert isinstance(names, tuple)

    structure = {
        path.stem: Image.open(path).convert("RGB")
        for path in (output_root / "structure").glob("*.png")
    }
    ground = {
        Path(name).stem: Image.open(output_root / name).convert("RGB")
        for name in names
    }
    prefix = "ice" if stage_name == "ice" else "basalt"
    room_tile = ground[f"{prefix}_plain"]
    path_tile = ground[f"{prefix}_path"]

    columns, rows = 20, 12
    walkable: set[tuple[int, int]] = set()
    corridor: set[tuple[int, int]] = set()
    for y in range(2, 10):
        for x in range(1, 8):
            walkable.add((x, y))
    for y in range(1, 11):
        for x in range(12, 19):
            walkable.add((x, y))
    for y in range(5, 8):
        for x in range(8, 12):
            walkable.add((x, y))
            corridor.add((x, y))
    walkable.remove((15, 5))

    preview = Image.new("RGB", (columns * TILE_SIZE, rows * TILE_SIZE))
    for y in range(rows):
        for x in range(columns):
            if (x, y) in walkable:
                tile = path_tile if (x, y) in corridor else room_tile
            else:
                cardinal = 0
                cardinal |= NORTH if (x, y - 1) in walkable else 0
                cardinal |= EAST if (x + 1, y) in walkable else 0
                cardinal |= SOUTH if (x, y + 1) in walkable else 0
                cardinal |= WEST if (x - 1, y) in walkable else 0
                diagonal = 0
                diagonal |= NORTH_EAST if (x + 1, y - 1) in walkable else 0
                diagonal |= SOUTH_EAST if (x + 1, y + 1) in walkable else 0
                diagonal |= SOUTH_WEST if (x - 1, y + 1) in walkable else 0
                diagonal |= NORTH_WEST if (x - 1, y - 1) in walkable else 0
                tile_name, rotation = classify_structure(cardinal, diagonal, x, y)
                tile = structure[tile_name]
                if rotation:
                    tile = tile.rotate(-rotation, resample=Image.Resampling.NEAREST)
            preview.paste(tile, (x * TILE_SIZE, y * TILE_SIZE))

    preview.save(
        output_root / f"{stage_name}_dungeon_preview.png",
        format="PNG",
        optimize=True,
    )


def main() -> None:
    for stage in STAGES.values():
        build_stage(stage)
    build_stage_structures()
    build_preview()
    for stage_name, stage in STAGES.items():
        build_dungeon_preview(stage_name, stage)
    print(f"Rebuilt 8 stage tiles, 18 stage structures, and wrote {PREVIEW.name}")


if __name__ == "__main__":
    main()
