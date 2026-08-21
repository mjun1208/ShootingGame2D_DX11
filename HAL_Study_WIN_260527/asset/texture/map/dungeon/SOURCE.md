# Dungeon tiles

`Dungeon_Tileset.png` is the source sheet supplied for this project. The files
under `ground/`, `structure/`, and `decor/` are 16x16 crops from that sheet,
kept separate so the existing instanced renderer can batch them efficiently.

The active room set is under `room/` and uses `(row, column)` coordinates:

- row 0: top-left, four top variants, top-right
- rows 1-3: three left variants, twelve floor variants, three right variants
- row 4: bottom-left, four bottom variants, bottom-right
- row 5: columns 3 and 0 are the left/right concave corners for downward passages

Upward passage corners reuse the four top wall variants at row 0, columns 1-4.
Top walls occasionally overlay the torch at row 9, column 0. Dungeon lighting is
currently disabled so tiles render at their original brightness.

Encounter barriers use row 3 columns 6-7 for horizontal gates. Both left and
right vertical gates alternate row 4 column 6 and row 5 column 6. Their visual
centers stay snapped to corridor cell centers while collision stays at room edges.
