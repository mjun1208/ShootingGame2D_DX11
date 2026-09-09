"""Rebuild title layers on an 8px grid with a shared palette and less texture noise.

Requires Pillow and numpy. Run from any directory; v1 originals stay untouched.
"""

from pathlib import Path
import json

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent
REPORT = ROOT.parents[3] / "review-build" / "title-pixel-preview"
LAYERS = ("far", "middle", "front")
SIZE = (2048, 1152)
LOGICAL_SIZE = (256, 144)
SCALE = 8
COLORS = 24


def composite(images):
    result = Image.new("RGBA", SIZE)
    for image in images:
        result = Image.alpha_composite(result, image)
    return result.convert("RGB")


def main():
    REPORT.mkdir(parents=True, exist_ok=True)
    originals = [Image.open(ROOT / f"title_bg_{name}_v1.png").convert("RGBA") for name in LAYERS]
    assert all(image.size == SIZE for image in originals)
    # Pillow resizes RGBA with premultiplied alpha so transparent black does not
    # darken the stone edges. Quantize alpha separately to retain hard cutouts.
    small = [image.resize(LOGICAL_SIZE, Image.Resampling.BOX) for image in originals]
    samples = []
    for image in small:
        pixels = np.asarray(image)
        samples.append(pixels[:, :, :3][pixels[:, :, 3] >= 128])
    visible = np.concatenate(samples)
    training = Image.fromarray(visible.reshape(1, -1, 3))
    palette = training.quantize(colors=COLORS, method=Image.Quantize.MEDIANCUT)
    outputs = []
    stats = {}
    for name, original, image in zip(LAYERS, originals, small):
        alpha = np.where(np.asarray(image)[:, :, 3] >= 128, 255, 0).astype(np.uint8)
        indexed = image.convert("RGB").quantize(palette=palette, dither=Image.Dither.NONE)
        indices = np.array(indexed)
        cleaned = indices.copy()
        removed = 0
        # Remove isolated flecks only when at least five opaque neighbors agree.
        # Do not modify silhouettes or spread colors across transparent regions.
        for y in range(1, indices.shape[0] - 1):
            for x in range(1, indices.shape[1] - 1):
                if not np.all(alpha[y - 1:y + 2, x - 1:x + 2] == 255):
                    continue
                neighbors = np.delete(indices[y - 1:y + 2, x - 1:x + 2].ravel(), 4)
                if np.count_nonzero(neighbors == indices[y, x]) > 1:
                    continue
                counts = np.bincount(neighbors, minlength=256)
                dominant = int(counts.argmax())
                if counts[dominant] >= 5:
                    cleaned[y, x] = dominant
                    removed += 1
        logical = Image.fromarray(cleaned).convert("P")
        logical.putpalette(palette.getpalette())
        logical = logical.convert("RGBA")
        logical.putalpha(Image.fromarray(alpha))
        pixels = np.array(logical)
        pixels[alpha == 0] = 0
        logical = Image.fromarray(pixels)
        output = logical.resize(SIZE, Image.Resampling.NEAREST)
        output.save(ROOT / f"title_bg_{name}_v2.png")
        outputs.append(output)
        actual = np.asarray(output)
        assert np.array_equal(actual, np.repeat(np.repeat(actual[::SCALE, ::SCALE], SCALE, 0), SCALE, 1))
        assert set(np.unique(actual[:, :, 3])) <= {0, 255}
        colors = np.unique(actual[:, :, :3][actual[:, :, 3] > 0], axis=0)
        assert len(colors) <= COLORS
        if name == "far":
            assert np.all(actual[:, :, 3] == 255)
        stats[name] = {"size": SIZE, "grid": SCALE, "opaque_colors": len(colors),
                       "isolated_pixels_removed": removed,
                       "original_coverage": float(np.mean(np.asarray(original)[:, :, 3] > 0)),
                       "new_coverage": float(np.mean(actual[:, :, 3] > 0))}
    before, after = composite(originals), composite(outputs)
    after.save(REPORT / "title_background_v2_composite.png")
    # Full-size detail crops prevent the preview itself from blurring the grid.
    comparison = Image.new("RGB", (1536, 840), (24, 24, 28))
    draw = ImageDraw.Draw(comparison)
    draw.text((16, 12), "BEFORE - original 4px grid", fill="white")
    draw.text((784, 12), "AFTER - 8px grid / shared 24-color palette", fill="white")
    for offset, image in ((0, before), (768, after)):
        comparison.paste(image.resize((768, 432), Image.Resampling.NEAREST), (offset, 40))
        comparison.paste(image.crop((64, 96, 832, 432)), (offset, 496))
    comparison.save(REPORT / "title_background_comparison.png")
    (REPORT / "pixel_validation.json").write_text(json.dumps(stats, indent=2), encoding="utf-8")
    print(json.dumps(stats, indent=2))


if __name__ == "__main__":
    main()
