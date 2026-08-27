param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\asset\sound\shotgun-blast-procedural.wav')
)

$sampleRate = 44100
$durationSeconds = 0.52
$sampleCount = [int]($sampleRate * $durationSeconds)
$samples = [double[]]::new($sampleCount)
$random = [System.Random]::new(260527)
$lowNoise = 0.0
$previousNoise = 0.0

for ($i = 0; $i -lt $sampleCount; $i++) {
    $time = $i / [double]$sampleRate
    $noise = $random.NextDouble() * 2.0 - 1.0
    $lowNoise += 0.16 * ($noise - $lowNoise)
    $highNoise = $noise - $previousNoise
    $previousNoise = $noise

    # Fast, bright pressure crack at the muzzle.
    $crackEnvelope = [Math]::Exp(-92.0 * $time)
    $crack = $highNoise * $crackEnvelope * 0.72

    # Dense mid-frequency blast carrying the recognizable shotgun body.
    $bodyEnvelope = [Math]::Exp(-19.0 * $time)
    $body = ($noise * 0.48 + $lowNoise * 0.52) * $bodyEnvelope * 0.82

    # Short downward-pitched low-frequency punch.
    $thumpFrequency = 128.0 - 72.0 * [Math]::Min(1.0, $time / $durationSeconds)
    $thump = [Math]::Sin(2.0 * [Math]::PI * $thumpFrequency * $time) *
        [Math]::Exp(-15.0 * $time) * 0.58

    # A small mechanical snap keeps the start from reading as an explosion.
    $snap = [Math]::Sin(2.0 * [Math]::PI * 1850.0 * $time) *
        [Math]::Exp(-125.0 * $time) * 0.20

    $sample = $crack + $body + $thump + $snap

    # Two subtle room reflections give the shot a compact tail.
    $reflectionA = $i - [int](0.057 * $sampleRate)
    $reflectionB = $i - [int](0.103 * $sampleRate)
    if ($reflectionA -ge 0) { $sample += $samples[$reflectionA] * 0.18 }
    if ($reflectionB -ge 0) { $sample += $samples[$reflectionB] * 0.09 }

    $samples[$i] = [Math]::Tanh($sample * 1.22) * 0.92
}

$outputDirectory = Split-Path -Parent $OutputPath
[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
$stream = [System.IO.File]::Create($OutputPath)
$writer = [System.IO.BinaryWriter]::new($stream)
try {
    $dataSize = $sampleCount * 2
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes('RIFF'))
    $writer.Write(36 + $dataSize)
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes('WAVE'))
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes('fmt '))
    $writer.Write(16)
    $writer.Write([int16]1)
    $writer.Write([int16]1)
    $writer.Write($sampleRate)
    $writer.Write($sampleRate * 2)
    $writer.Write([int16]2)
    $writer.Write([int16]16)
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes('data'))
    $writer.Write($dataSize)
    foreach ($sample in $samples) {
        $pcm = [int16][Math]::Round([Math]::Max(-1.0, [Math]::Min(1.0, $sample)) * 32767.0)
        $writer.Write($pcm)
    }
}
finally {
    $writer.Dispose()
    $stream.Dispose()
}

Write-Output $OutputPath
