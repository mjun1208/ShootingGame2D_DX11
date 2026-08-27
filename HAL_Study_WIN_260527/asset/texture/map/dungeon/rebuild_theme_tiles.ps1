$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$dungeonRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourcePath = Join-Path $dungeonRoot 'Dungeon_Tileset.png'
$themesRoot = Join-Path $dungeonRoot 'themes'
$tileSize = 16

$roomCrops = [ordered]@{
    'top_left' = @(0, 0); 'top_01' = @(1, 0); 'top_02' = @(2, 0)
    'top_03' = @(3, 0); 'top_04' = @(4, 0); 'top_right' = @(5, 0)
    'left_01' = @(0, 1); 'left_02' = @(0, 2); 'left_03' = @(0, 3)
    'floor_01' = @(1, 1); 'floor_02' = @(2, 1); 'floor_03' = @(3, 1)
    'floor_04' = @(4, 1); 'floor_05' = @(1, 2); 'floor_06' = @(2, 2)
    'floor_07' = @(3, 2); 'floor_08' = @(4, 2); 'floor_09' = @(1, 3)
    'floor_10' = @(2, 3); 'floor_11' = @(3, 3); 'floor_12' = @(4, 3)
    'right_01' = @(5, 1); 'right_02' = @(5, 2); 'right_03' = @(5, 3)
    'bottom_left' = @(0, 4); 'bottom_01' = @(1, 4); 'bottom_02' = @(2, 4)
    'bottom_03' = @(3, 4); 'bottom_04' = @(4, 4); 'bottom_right' = @(5, 4)
    'concave_down_column_0' = @(0, 5); 'concave_down_column_3' = @(3, 5)
    'void_deep' = @(6, 0); 'void_mottle' = @(7, 0)
}

$decorCrops = [ordered]@{
    'torch' = @(0, 9); 'candle' = @(3, 9); 'coin' = @(6, 8)
    'pot_red' = @(9, 8); 'pot_blue' = @(7, 8); 'bones' = @(8, 6)
    'skull' = @(7, 7); 'chest' = @(4, 8)
}

function Clamp-Byte {
    param([double] $Value)
    return [byte][Math]::Max(0, [Math]::Min(255, [Math]::Round($Value)))
}

# The source uses a compact authored palette. Mapping its dominant colors
# explicitly keeps walls pale and readable while turning the room interiors
# into bright spring grass. Unknown neutral shades still use the fallback
# conversion below, so the script remains safe if minor source details change.
$forestPalette = @{
    '37,19,26' = @(24, 63, 42)
    '54,32,48' = @(52, 142, 61)
    '61,37,59' = @(72, 166, 69)
    '66,44,59' = @(98, 145, 99)
    '72,48,64' = @(110, 154, 107)
    '76,47,73' = @(119, 158, 111)
    '79,55,64' = @(132, 168, 120)
    '84,55,64' = @(149, 183, 131)
    '89,55,86' = @(143, 178, 131)
    '110,74,72' = @(192, 214, 174)
    '120,81,79' = @(222, 233, 199)
}

$forestDecorPalette = @{
    'd' = [System.Drawing.Color]::FromArgb(255, 24, 79, 48)
    'g' = [System.Drawing.Color]::FromArgb(255, 42, 132, 58)
    'l' = [System.Drawing.Color]::FromArgb(255, 91, 190, 70)
    'w' = [System.Drawing.Color]::FromArgb(255, 255, 246, 190)
    'y' = [System.Drawing.Color]::FromArgb(255, 250, 204, 56)
    'p' = [System.Drawing.Color]::FromArgb(255, 239, 103, 127)
    'b' = [System.Drawing.Color]::FromArgb(255, 91, 174, 226)
    'r' = [System.Drawing.Color]::FromArgb(255, 218, 76, 67)
    'c' = [System.Drawing.Color]::FromArgb(255, 232, 197, 132)
}

# These patterns reuse the shared decor slots without changing runtime enums:
# wall vine, daisies, yellow flowers, tulips, blue flowers, fern, mushrooms,
# and a mixed flower bush. Each pattern is centered in its 16x16 crop.
$forestDecorPatterns = [ordered]@{
    'torch' = @(
        '..gg....gg..', '.gllg..gllg.', '..gg....gg..', '...g....g...',
        '...g.ww.g...', '..glwyywlg..', '...g.ww.g...', '...g....g...',
        '..glg..glg..', '.gllg..gllg.', '..gg....gg..', '............')
    'candle' = @(
        '....ww......', '...wyyw.ww..', '...ww..wyyw.', '....g...ww..',
        '..wwg..gl...', '.wyyw.gllg..', '..ww..glg...', '...g.gllg...',
        '..gllgllg...', '...ggggg....', '............', '............')
    'coin' = @(
        '..yy....yy..', '.yyyy..yyyy.', '..yy....yy..', '...g.yy.g...',
        '..g.yyyy.g..', '.yyg..g..g..', 'yyyy.gllg...', '.yy.gllllg..',
        '..gllllllg..', '...gggggg...', '............', '............')
    'pot_red' = @(
        '.pp....pp...', 'pppp..pppp..', '.pp....pp...', '..g....g....',
        '..g.pp.g....', '.g.pppp.g...', '.g..pp..g...', '..gllllg....',
        '.gllllllg...', '..gggggg....', '............', '............')
    'pot_blue' = @(
        '..bb....bb..', '.bbbb..bbbb.', '..bb....bb..', '...g.bb.g...',
        '..g.bbbb.g..', '.g...bb..g..', '.gl..g..lg..', '..gllllllg..',
        '.gllllllllg.', '..gggggggg..', '............', '............')
    'bones' = @(
        '......lg....', '....lgllg...', '..lgllllg...', '.glllllg....',
        '...glllllg..', '.glllllg....', '..gllllllg..', '....gllllg..',
        '..gllllg....', '...ggggg....', '............', '............')
    'skull' = @(
        '...rrrrr....', '..rrwrwrr...', '.rrrrrrrrr..', '..rrrrrrr...',
        '....cc......', '.rr.cc.rr...', 'rrrrccrrrr..', '.rr.cc.rr...',
        '..gllllg....', '.gllllllg...', '..gggggg....', '............')
    'chest' = @(
        '..p..ww..b..', '.ppp.wyywbbb.', '..p...ww..b..', '...g..g..g..',
        '.yyg.glg.g..', 'yyyyglllg...', '.yyglllllg..', '..gllllllg..',
        '.gllllllllg.', '..gggggggg..', '............', '............')
}

function Draw-ForestDecoration {
    param(
        [System.Drawing.Bitmap] $Sheet,
        [int] $TileX,
        [int] $TileY,
        [string[]] $Pattern
    )

    $transparent = [System.Drawing.Color]::FromArgb(0, 0, 0, 0)
    for ($y = 0; $y -lt $tileSize; ++$y) {
        for ($x = 0; $x -lt $tileSize; ++$x) {
            $Sheet.SetPixel($TileX * $tileSize + $x, $TileY * $tileSize + $y, $transparent)
        }
    }

    if ($Pattern.Count -gt $tileSize) { throw 'Forest decor pattern is too tall.' }
    $patternWidth = ($Pattern | ForEach-Object { $_.Length } | Measure-Object -Maximum).Maximum
    if ($patternWidth -gt $tileSize) { throw 'Forest decor pattern is too wide.' }

    $offsetY = $tileSize - $Pattern.Count - 1
    for ($y = 0; $y -lt $Pattern.Count; ++$y) {
        $rowWidth = $Pattern[$y].Length
        $offsetX = [int][Math]::Floor(($tileSize - $rowWidth) * 0.5)
        for ($x = 0; $x -lt $rowWidth; ++$x) {
            $key = [string]$Pattern[$y][$x]
            if ($key -eq '.') { continue }
            if (-not $forestDecorPalette.ContainsKey($key)) {
                throw "Unknown forest decor palette key: $key"
            }
            $Sheet.SetPixel(
                $TileX * $tileSize + $offsetX + $x,
                $TileY * $tileSize + $offsetY + $y,
                $forestDecorPalette[$key])
        }
    }
}

function Convert-ThemePixel {
    param(
        [System.Drawing.Color] $Color,
        [ValidateSet('forest', 'crypt')][string] $Theme,
        [int] $X,
        [int] $Y
    )

    if ($Color.A -eq 0) { return $Color }

    $maximum = [Math]::Max($Color.R, [Math]::Max($Color.G, $Color.B))
    $minimum = [Math]::Min($Color.R, [Math]::Min($Color.G, $Color.B))
    $range = $maximum - $minimum
    $isAccent = $range -ge 55 -and (
        $Color.R -gt $Color.G * 1.35 -or
        $Color.B -gt $Color.R * 1.25 -or
        ($Color.R -gt 120 -and $Color.G -gt 75 -and $Color.B -lt 90))
    if ($isAccent) { return $Color }

    $luminance = $Color.R * 0.299 + $Color.G * 0.587 + $Color.B * 0.114
    if ($Theme -eq 'forest') {
        $paletteKey = '{0},{1},{2}' -f $Color.R, $Color.G, $Color.B
        if ($forestPalette.ContainsKey($paletteKey)) {
            $mapped = $forestPalette[$paletteKey]
            $red = [byte]$mapped[0]
            $green = [byte]$mapped[1]
            $blue = [byte]$mapped[2]
        }
        else {
            $red = Clamp-Byte ($luminance * 0.78 + 18)
            $green = Clamp-Byte ($luminance * 1.08 + 34)
            $blue = Clamp-Byte ($luminance * 0.67 + 18)
        }

        # Sparse sunlit leaf catches, restricted to authored opaque pixels.
        $mossHash = (($X * 17 + $Y * 31 + $X * $Y * 3) -band 0x7fffffff) % 47
        if ($mossHash -eq 0 -and $luminance -gt 45) {
            $red = Clamp-Byte ($red + 18)
            $green = Clamp-Byte ($green + 24)
            $blue = Clamp-Byte ($blue + 7)
        }
    }
    else {
        $red = Clamp-Byte ($luminance * 0.70 + 6)
        $green = Clamp-Byte ($luminance * 0.82 + 12)
        $blue = Clamp-Byte ($luminance * 0.95 + 18)
    }
    return [System.Drawing.Color]::FromArgb($Color.A, $red, $green, $blue)
}

function Save-CropSet {
    param(
        [System.Drawing.Bitmap] $Sheet,
        [System.Collections.IDictionary] $Crops,
        [string] $OutputDirectory
    )

    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    foreach ($entry in $Crops.GetEnumerator()) {
        $coordinates = $entry.Value
        $rectangle = New-Object System.Drawing.Rectangle `
            ($coordinates[0] * $tileSize), ($coordinates[1] * $tileSize), $tileSize, $tileSize
        $tile = $Sheet.Clone($rectangle, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $tile.Save(
                (Join-Path $OutputDirectory ($entry.Key + '.png')),
                [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $tile.Dispose()
        }
    }
}

$source = [System.Drawing.Bitmap]::FromFile($sourcePath)
try {
    foreach ($theme in @('forest', 'crypt')) {
        $themeRoot = Join-Path $themesRoot $theme
        New-Item -ItemType Directory -Force -Path $themeRoot | Out-Null
        $sheet = New-Object System.Drawing.Bitmap `
            $source.Width, $source.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            for ($y = 0; $y -lt $source.Height; ++$y) {
                for ($x = 0; $x -lt $source.Width; ++$x) {
                    $sheet.SetPixel($x, $y, (Convert-ThemePixel $source.GetPixel($x, $y) $theme $x $y))
                }
            }
            if ($theme -eq 'forest') {
                foreach ($entry in $decorCrops.GetEnumerator()) {
                    $coordinates = $entry.Value
                    Draw-ForestDecoration $sheet `
                        $coordinates[0] $coordinates[1] $forestDecorPatterns[$entry.Key]
                }
            }
            $sheet.Save(
                (Join-Path $themeRoot ('Dungeon_Tileset_{0}.png' -f $theme)),
                [System.Drawing.Imaging.ImageFormat]::Png)
            Save-CropSet $sheet $roomCrops (Join-Path $themeRoot 'room')
            Save-CropSet $sheet $decorCrops (Join-Path $themeRoot 'decor')
        }
        finally {
            $sheet.Dispose()
        }
    }
}
finally {
    $source.Dispose()
}

Write-Host "Rebuilt forest and crypt themes under $themesRoot"
