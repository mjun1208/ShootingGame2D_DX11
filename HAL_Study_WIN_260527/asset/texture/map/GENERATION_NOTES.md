# Procedural forest map assets

These original map assets were generated with the built-in ImageGen workflow and
then cropped, resized, and alpha-validated for this DirectX 11 project. The runtime
loads the individual PNG files under `ground/` and `decor/`; the atlas images are
kept only as editable source material.

The art direction takes only broad inspiration from cute top-down fantasy
roguelites: rounded natural shapes, dark blue-gray outlines, readable combat
surfaces, clustered foliage, and warm earth paths. It does not copy characters,
UI, layouts, or tile artwork from another game.

## Ground atlas prompt (current clean pass)

The current source is `forest_ground_atlas_source.png`. Run
`rebuild_ground_tiles.py` to split it into the sixteen runtime files. The script
exports 60 x 60 opaque RGB PNGs, limits each tile to sixteen colors, removes
color-profile metadata, and matches opposite edge pixels so point-sampled repeats
do not expose a hard seam. It also normalizes each variant to its base material's
average color and flattens the main dirt-path tile; the carved corridor silhouette,
not a repeated stripe inside every cell, defines the path.

```text
Edit the provided 4 x 4 ground-texture atlas while preserving its exact square
canvas, exact 4-column x 4-row layout, row order, palette families, top-down view,
and opaque pixel-art presentation.

Make the atlas dramatically calmer for fast combat. Remove at least 80 percent of
the repeated grass, soil, and water marks. Grass should be nearly flat with only a
few low-contrast marks or one broad organic tone patch. Dirt should be mostly flat;
the worn path should be broad and readable with soft edges. Water should be nearly
flat with only one or two wide ripple bands. Stone should use a handful of large,
quiet shapes or two thin cracks. Forest floor should be calm, with no more than one
or two broad, low-contrast root silhouettes.

Maintain original chunky low-resolution pixel art and broad color fields. No grid
lines, gutters, labels, borders, objects, characters, UI, text, watermark,
directional lighting, or cast shadows. Each cell remains edge-to-edge and
independently seamless. Do not copy another game's artwork.
```

The clean pass intentionally keeps room floors about 96 percent plain, corridors
about 94 percent worn path, and moves most environmental detail to sparse boundary
props. This produces the large readable color blocks used by the current dungeon.

## Previous ground atlas prompt

```text
Use case: stylized-concept
Asset type: production source atlas for 2D game environment tiles, to be split into sixteen square tiles
Primary request: create an ORIGINAL exact 4 by 4 sprite sheet of seamless top-down ground tiles for a cozy forest fantasy action roguelite
Scene/backdrop: tile textures only, no scene composition
Subject and fixed cell layout:
Row 1: four meadow tiles — plain soft grass, grass with tiny clover, grass with sparse tiny flowers, darker mossy grass
Row 2: four earth tiles — warm plain soil, soil with tiny pebbles, worn earthen trail, dry leaf-litter soil
Row 3: four water tiles — calm deep teal water, teal water with subtle ripples, pale shallow water, shallow water with sparse reeds at the edges
Row 4: four forest-floor and stone tiles — mossy cobblestone, cracked gray-blue stone, pine-needle forest floor, subtle exposed-root forest floor
Style/medium: crisp hand-authored pixel art; rounded organic marks; strong but restrained dark blue-gray outlines; compact readable shapes; warm charming fantasy mood; medium natural saturation; original visual language, not a copy of any existing game's assets
Composition/framing: orthographic true top-down; EXACT 4 columns by 4 rows; every cell exactly the same square size; atlas filled edge-to-edge; no gutter, no grid line, no labels
Color palette: dark blue-gray #28343B, forest green #365C49, grass green #55A765, light olive #91AD61, warm earth #B77A42, stone #78838D, cream highlights #F1E3B5
Materials/textures: chunky pixel clusters, subtle value variation, low visual noise so combat remains readable
Constraints: each of the sixteen cells must be independently seamless on all four edges; tile texture must reach every cell edge; no objects crossing into neighboring cells; no perspective; no lighting direction or cast shadows; no characters; no UI; no text; no symbols; no logos; no watermark; no border; no padding; no grid separators; use only fully opaque pixels
```

## Dungeon structure atlas prompt

The inaccessible-space source is `dungeon_structure_atlas_source.png`. Run
`rebuild_structure_tiles.py` to split it into nine opaque 60 x 60 RGB textures
under `structure/` and rebuild `cliff_map_style_preview.png`. The runtime void is
normalized to `#02101F`, matching the generated cliff-cell background so the
solid region does not reveal a one-tile rectangular halo.

```text
Use case: stylized-concept
Asset type: production source atlas for opaque 2D top-down dungeon structure tiles
Primary request: create an ORIGINAL exact 3 by 3 sprite sheet of bold, low-noise
abyss and cliff boundary tiles for a fast bullet-hell roguelite. The tiles must
make walkable floor islands unmistakably separate from inaccessible space.

Layout: abyss fill A; abyss fill B; straight cliff A / straight cliff B;
two-side corner; diagonal-only corner / opposite-side channel; three-side cap;
four-side pillar. Canonical straight tiles open toward the bottom. The two-side
corner opens toward bottom and right. The diagonal corner touches walkable floor
only beyond its bottom-right corner. Channel, cap, and pillar open toward the
directions implied by their names.

Style: chunky low-resolution pixel art, broad flat color blocks, deep blue-black
abyss, slate cliff faces, pale stone lips, restrained moss, neutral ambient light.
Every tile is fully opaque and remains valid after 90-degree rotation. Avoid dense
cracks, pebble noise, gradients, directional cast shadows, focal symbols, text,
logos, and watermarks. Do not copy another game's artwork.
```

The selected atlas received one precise edit pass: visible sheet dividers were
removed, the diagonal-only corner was reduced to a compact ledge, and obstacle top
surfaces were darkened so no solid cell resembles walkable floor.

## Decoration atlas prompt

```text
Use case: stylized-concept
Asset type: production source sprite atlas for small 2D game map decorations, later converted to transparent sprites
Primary request: create an ORIGINAL exact 4 by 4 sprite sheet containing sixteen separate top-down forest-fantasy map props, exactly one centered prop cluster per cell
Scene/backdrop: perfectly flat solid #ff00ff chroma-key background for local background removal; the background must be one uniform color with no shadows, gradients, texture, reflections, floor plane, or lighting variation
Subject and fixed cell layout:
Row 1: tiny cream-and-yellow wildflower cluster; tiny blue wildflower cluster; clover tuft; long grass tuft
Row 2: small rounded pebble cluster; small mossy rock; three tiny mushrooms; dry leaf pile
Row 3: fern; compact round shrub; low tree stump; short fallen log
Row 4: reed tuft; small weathered rune stone with only an abstract spiral mark; berry bush; small rounded young tree
Style/medium: crisp hand-authored pixel art; orthographic true top-down/three-quarter top-down props; rounded organic silhouettes; restrained dark blue-gray outlines; warm charming fantasy mood; medium natural saturation; original visual language, not a copy of any existing game's assets
Composition/framing: EXACT 4 columns by 4 rows; every cell exactly the same square size; one isolated sprite centered in each cell; generous empty chroma-key padding around every sprite; no sprite touches a cell boundary; no sprite crosses into another cell
Color palette: dark blue-gray #28343B, forest green #365C49, grass green #55A765, light olive #91AD61, earth #B77A42, stone #78838D, cream #F1E3B5
Constraints: do not use #ff00ff or any magenta anywhere in any prop; crisp pixel edges; no cast shadow; no contact shadow; no glow; no semi-transparent smoke; no perspective scene; no characters; no UI; no text; no letters; no numbers; no logos; no watermark; no visible grid lines or cell borders
```

The decoration atlas was converted to alpha with the ImageGen skill's chroma-key
removal helper, then every sprite was trimmed and saved as a straight-alpha RGBA
PNG. Final validation checks cover transparent corners, alpha range, magenta fringe,
runtime path matching, and Windows WIC decoding.
