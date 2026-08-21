from pathlib import Path

from PIL import Image, ImageStat


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "dungeon_structure_atlas_source.png"
OUTPUT_DIR = ROOT / "structure"
PREVIEW = ROOT / "cliff_map_style_preview.png"
TILE_SIZE = 60
VOID_COLOR = (2, 16, 31)

TILE_NAMES = (
    ("void_deep.png", "void_mottle.png", "cliff_edge.png"),
    ("cliff_edge_moss.png", "cliff_inner_corner.png", "cliff_outer_corner.png"),
    ("cliff_channel.png", "cliff_cap.png", "cliff_pillar.png"),
)

NORTH = 0x01
EAST = 0x02
SOUTH = 0x04
WEST = 0x08
NORTH_EAST = 0x01
SOUTH_EAST = 0x02
SOUTH_WEST = 0x04
NORTH_WEST = 0x08


def soften_to_void_edges(image: Image.Image, blend_width: int = 5) -> Image.Image:
    image = image.convert("RGB")
    pixels = image.load()
    width, height = image.size
    for offset in range(blend_width):
        strength = (blend_width - offset) / blend_width
        for y in range(height):
            for x in (offset, width - 1 - offset):
                current = pixels[x, y]
                pixels[x, y] = tuple(
                    round(current[channel] * (1.0 - strength) + VOID_COLOR[channel] * strength)
                    for channel in range(3)
                )
        for x in range(width):
            for y in (offset, height - 1 - offset):
                current = pixels[x, y]
                pixels[x, y] = tuple(
                    round(current[channel] * (1.0 - strength) + VOID_COLOR[channel] * strength)
                    for channel in range(3)
                )
    return image


def normalize_mean(image: Image.Image, target: tuple[int, int, int]) -> Image.Image:
    image = image.convert("RGB")
    source_mean = ImageStat.Stat(image).mean
    channels = []
    for band, source, wanted in zip(image.split(), source_mean, target):
        delta = wanted - source
        channels.append(band.point(
            lambda value, offset=delta: max(0, min(255, round(value + offset)))
        ))
    return Image.merge("RGB", channels)


def save_runtime_tile(image: Image.Image, path: Path, colors: int = 32) -> None:
    image = image.convert("RGB").quantize(
        colors=colors,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    ).convert("RGB")
    image.save(path, format="PNG", optimize=True)


def bit_count(mask: int) -> int:
    return sum(1 for bit in (NORTH, EAST, SOUTH, WEST) if mask & bit)


def classify_structure(cardinal: int, diagonal: int, x: int, y: int) -> tuple[str, int]:
    count = bit_count(cardinal)
    if count == 0:
        diagonal_count = bit_count(diagonal)
        if diagonal_count == 1:
            rotations = {
                SOUTH_EAST: 0,
                SOUTH_WEST: 90,
                NORTH_WEST: 180,
                NORTH_EAST: 270,
            }
            return "cliff_outer_corner", rotations[diagonal]
        return ("void_mottle" if (x * 17 + y * 29) % 23 == 0 else "void_deep"), 0

    if count == 1:
        rotations = {SOUTH: 0, WEST: 90, NORTH: 180, EAST: 270}
        name = "cliff_edge_moss" if (x * 11 + y * 7) % 6 == 0 else "cliff_edge"
        return name, rotations[cardinal]

    if count == 2:
        if cardinal == (EAST | WEST):
            return "cliff_channel", 0
        if cardinal == (NORTH | SOUTH):
            return "cliff_channel", 90
        rotations = {
            SOUTH | EAST: 0,
            SOUTH | WEST: 90,
            NORTH | WEST: 180,
            NORTH | EAST: 270,
        }
        return "cliff_inner_corner", rotations[cardinal]

    if count == 3:
        rotations = {
            EAST | SOUTH | WEST: 0,
            NORTH | SOUTH | WEST: 90,
            NORTH | EAST | WEST: 180,
            NORTH | EAST | SOUTH: 270,
        }
        return "cliff_cap", rotations[cardinal]

    return "cliff_pillar", 0


def build_preview() -> None:
    structure = {
        path.stem: Image.open(path).convert("RGB")
        for path in OUTPUT_DIR.glob("*.png")
    }
    ground_dir = ROOT / "ground"
    grass = Image.open(ground_dir / "grass_plain.png").convert("RGB")
    dirt = Image.open(ground_dir / "dirt_path.png").convert("RGB")

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

    preview = Image.new("RGB", (columns * TILE_SIZE, rows * TILE_SIZE), VOID_COLOR)
    for y in range(rows):
        for x in range(columns):
            position = (x, y)
            if position in walkable:
                tile = dirt if position in corridor else grass
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
                name, rotation = classify_structure(cardinal, diagonal, x, y)
                tile = structure[name]
                if rotation:
                    tile = tile.rotate(-rotation, resample=Image.Resampling.NEAREST)
            preview.paste(tile, (x * TILE_SIZE, y * TILE_SIZE))

    preview.save(PREVIEW, format="PNG", optimize=True)


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with Image.open(SOURCE) as atlas_source:
        atlas = atlas_source.convert("RGB")
    width, height = atlas.size
    if width != height or width % 3 != 0:
        raise ValueError(f"Structure atlas must be a square 3x3 sheet, got {width}x{height}")

    for row, names in enumerate(TILE_NAMES):
        top = row * height // 3
        bottom = (row + 1) * height // 3
        for column, name in enumerate(names):
            left = column * width // 3
            right = (column + 1) * width // 3
            tile = atlas.crop((left, top, right, bottom))
            tile = tile.resize((TILE_SIZE, TILE_SIZE), Image.Resampling.LANCZOS)
            if name == "void_deep.png":
                tile = Image.new("RGB", (TILE_SIZE, TILE_SIZE), VOID_COLOR)
                save_runtime_tile(tile, OUTPUT_DIR / name, colors=2)
            elif name == "void_mottle.png":
                tile = normalize_mean(tile, VOID_COLOR)
                tile = soften_to_void_edges(tile)
                save_runtime_tile(tile, OUTPUT_DIR / name, colors=12)
            else:
                save_runtime_tile(tile, OUTPUT_DIR / name, colors=32)

    build_preview()
    print(f"Rebuilt 9 structure tiles from {SOURCE.name} and wrote {PREVIEW.name}")


if __name__ == "__main__":
    main()
