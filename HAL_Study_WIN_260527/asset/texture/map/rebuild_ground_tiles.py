from pathlib import Path

from PIL import Image, ImageStat


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "forest_ground_atlas_source.png"
OUTPUT_DIR = ROOT / "ground"
TILE_SIZE = 60
PREVIEW = ROOT / "clean_map_style_preview.png"

TILE_NAMES = (
    ("grass_plain.png", "grass_clover.png", "grass_flowers.png", "grass_moss.png"),
    ("dirt_plain.png", "dirt_pebbles.png", "dirt_path.png", "dirt_leaves.png"),
    ("water_deep.png", "water_ripple.png", "water_shallow.png", "water_reeds.png"),
    ("stone_moss.png", "stone_cracked.png", "forest_needles.png", "forest_roots.png"),
)

COLOR_MATCH_PAIRS = (
    ("grass_clover.png", "grass_plain.png"),
    ("grass_flowers.png", "grass_plain.png"),
    ("dirt_pebbles.png", "dirt_plain.png"),
    ("dirt_leaves.png", "dirt_plain.png"),
    ("water_ripple.png", "water_deep.png"),
    ("water_reeds.png", "water_shallow.png"),
    ("stone_cracked.png", "stone_moss.png"),
    ("forest_roots.png", "forest_needles.png"),
)


def make_edges_periodic(image: Image.Image, blend_width: int = 5) -> Image.Image:
    """Match opposite outer pixels and soften the transition into each tile."""
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
            average = tuple((left[channel] + right[channel]) // 2 for channel in range(3))
            pixels[left_x, y] = tuple(
                round(left[channel] * (1.0 - strength) + average[channel] * strength)
                for channel in range(3)
            )
            pixels[right_x, y] = tuple(
                round(right[channel] * (1.0 - strength) + average[channel] * strength)
                for channel in range(3)
            )

    for offset in range(blend_width):
        strength = (blend_width - offset) / blend_width
        top_y = offset
        bottom_y = height - 1 - offset
        for x in range(width):
            top = pixels[x, top_y]
            bottom = pixels[x, bottom_y]
            average = tuple((top[channel] + bottom[channel]) // 2 for channel in range(3))
            pixels[x, top_y] = tuple(
                round(top[channel] * (1.0 - strength) + average[channel] * strength)
                for channel in range(3)
            )
            pixels[x, bottom_y] = tuple(
                round(bottom[channel] * (1.0 - strength) + average[channel] * strength)
                for channel in range(3)
            )

    return image


def match_average_color(image: Image.Image, reference: Image.Image) -> Image.Image:
    source_mean = ImageStat.Stat(image.convert("RGB")).mean
    target_mean = ImageStat.Stat(reference.convert("RGB")).mean
    adjusted_channels = []
    for channel, delta in zip(image.convert("RGB").split(),
                              (target - source for source, target in zip(source_mean, target_mean))):
        adjusted_channels.append(channel.point(
            lambda value, offset=delta: max(0, min(255, round(value + offset)))
        ))
    return Image.merge("RGB", adjusted_channels)


def normalize_variant_colors() -> None:
    for variant_name, reference_name in COLOR_MATCH_PAIRS:
        with Image.open(OUTPUT_DIR / variant_name) as variant_source:
            variant = variant_source.convert("RGB")
        with Image.open(OUTPUT_DIR / reference_name) as reference_source:
            reference = reference_source.convert("RGB")

        variant = match_average_color(variant, reference)
        variant = make_edges_periodic(variant)
        variant = variant.quantize(
            colors=16,
            method=Image.Quantize.MEDIANCUT,
            dither=Image.Dither.NONE,
        ).convert("RGB")
        variant.save(OUTPUT_DIR / variant_name, format="PNG", optimize=True)


def stabilize_path_profile() -> None:
    """Use a quiet fill; the carved corridor silhouette already defines the path."""
    path_file = OUTPUT_DIR / "dirt_path.png"
    with Image.open(path_file) as source:
        path = source.convert("RGB")

    mean = ImageStat.Stat(path).mean
    average = tuple(round(channel) for channel in mean)
    path = Image.new("RGB", path.size, average)

    path = make_edges_periodic(path)
    path = path.quantize(
        colors=16,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    ).convert("RGB")
    path.save(path_file, format="PNG", optimize=True)


def build_preview() -> None:
    tiles = {
        path.stem: Image.open(path).convert("RGB")
        for path in OUTPUT_DIR.glob("*.png")
    }
    columns, rows = 16, 10
    preview = Image.new("RGB", (columns * TILE_SIZE, rows * TILE_SIZE))

    for y in range(rows):
        for x in range(columns):
            tile_name = "forest_needles"
            rotation = 0

            if 1 <= x <= 6 and 2 <= y <= 7:
                tile_name = "grass_plain"
            elif 10 <= x <= 14 and 1 <= y <= 8:
                tile_name = "stone_moss"
            elif 7 <= x <= 9 and 4 <= y <= 5:
                tile_name = "dirt_path"
                rotation = 90

            if (x, y) == (3, 3):
                tile_name = "grass_clover"
            elif (x, y) == (5, 6):
                tile_name = "grass_flowers"
            elif (x, y) == (12, 6):
                tile_name = "stone_cracked"
            elif (x, y) in {(0, 4), (7, 3), (9, 6), (15, 5)}:
                tile_name = "forest_roots"

            tile = tiles[tile_name]
            if rotation:
                tile = tile.rotate(rotation, resample=Image.Resampling.NEAREST)
            preview.paste(tile, (x * TILE_SIZE, y * TILE_SIZE))

    decor_dir = ROOT / "decor"
    for name, cell_x, cell_y, scale in (
        ("young_tree.png", 0, 2, 0.82),
        ("shrub.png", 7, 2, 0.75),
        ("rock_moss.png", 9, 7, 0.72),
        ("young_tree.png", 15, 7, 0.82),
    ):
        with Image.open(decor_dir / name) as source:
            sprite = source.convert("RGBA")
            sprite = sprite.resize(
                (round(sprite.width * scale), round(sprite.height * scale)),
                Image.Resampling.NEAREST,
            )
            center_x = round((cell_x + 0.5) * TILE_SIZE)
            center_y = round((cell_y + 0.5) * TILE_SIZE)
            preview.paste(
                sprite,
                (center_x - sprite.width // 2, center_y - sprite.height // 2),
                sprite,
            )

    preview.save(PREVIEW, format="PNG", optimize=True)
def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with Image.open(SOURCE) as atlas:
        atlas = atlas.convert("RGB")
        width, height = atlas.size
        if width != height:
            raise ValueError(f"Ground atlas must be square, got {width}x{height}")

        for row, names in enumerate(TILE_NAMES):
            top = (row * height + 2) // 4
            bottom = ((row + 1) * height + 2) // 4
            for column, name in enumerate(names):
                left = (column * width + 2) // 4
                right = ((column + 1) * width + 2) // 4
                tile = atlas.crop((left, top, right, bottom))
                tile = tile.resize((TILE_SIZE, TILE_SIZE), Image.Resampling.LANCZOS)
                tile = make_edges_periodic(tile)
                tile = tile.quantize(
                    colors=16,
                    method=Image.Quantize.MEDIANCUT,
                    dither=Image.Dither.NONE,
                ).convert("RGB")
                tile.save(OUTPUT_DIR / name, format="PNG", optimize=True)

    normalize_variant_colors()
    stabilize_path_profile()
    build_preview()
    print(
        f"Rebuilt {sum(len(row) for row in TILE_NAMES)} ground tiles from "
        f"{SOURCE.name} and wrote {PREVIEW.name}"
    )


if __name__ == "__main__":
    main()
