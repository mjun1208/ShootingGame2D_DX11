# Dungeon tileset themes

Both runtime themes are deterministic palette-and-detail transforms of
`../Dungeon_Tileset.png`. They preserve its exact 160x160 canvas, 16x16 grid,
alpha channel, sprite positions, and existing procedural-map crop rules.

- `forest/`: mossy forest ruins used by round 1.
- `crypt/`: cold blue-gray crypt used by round 2.

Run `../rebuild_theme_tiles.ps1` after changing the source or palette rules.
Runtime assets use the exact-grid deterministic sheets stored here.
