# Convert-PngToIco.ps1: genera ICO multirresolución desde PNG.

# param: declara PNG fuente, ICO destino y tamaños.
param(
    [Parameter(Mandatory = $true)]
    [string] $InputPng,

    [Parameter(Mandatory = $true)]



    [string] $OutputIco,


    [int[]] $Sizes = @(16, 24, 32, 48, 64, 128, 256)
)

$ErrorActionPreference = 'Stop'



Set-StrictMode -Version 2.0

# Write-Info: escribe diagnóstico de conversión.
function Write-Info([string] $Message) {



    Write-Host "[ICONO] $Message"

}

# Get-IcoDimensionByte: codifica dimensiones ICO.
function Get-IcoDimensionByte([int] $Value) {
    if ($Value -eq 256) { return [byte]0 }
    if ($Value -lt 1 -or $Value -gt 255) { throw "Tamano ICO invalido: $Value" }
    return [byte]$Value



}

# New-ResampledSquarePngBytes: remuestrea el PNG en lienzo cuadrado.

function New-ResampledSquarePngBytes($SourceBitmap, [int] $Size) {
    if ($Size -lt 1 -or $Size -gt 256) { throw "Tamano de icono fuera de rango: $Size" }




    $bitmap = New-Object System.Drawing.Bitmap $Size, $Size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)



    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)

        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality



        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver




        $sourceWidth = [double]$SourceBitmap.Width

        $sourceHeight = [double]$SourceBitmap.Height
        if ($sourceWidth -le 0 -or $sourceHeight -le 0) { throw 'La imagen fuente no tiene dimensiones validas.' }

        $scale = [Math]::Min([double]$Size / $sourceWidth, [double]$Size / $sourceHeight)



        $drawWidth = [Math]::Max(1, [int][Math]::Round($sourceWidth * $scale))
        $drawHeight = [Math]::Max(1, [int][Math]::Round($sourceHeight * $scale))
        $drawX = [int][Math]::Floor(($Size - $drawWidth) / 2.0)

        $drawY = [int][Math]::Floor(($Size - $drawHeight) / 2.0)



        $destRect = New-Object System.Drawing.Rectangle $drawX, $drawY, $drawWidth, $drawHeight



        $srcRect = New-Object System.Drawing.Rectangle 0, 0, $SourceBitmap.Width, $SourceBitmap.Height
        $graphics.DrawImage($SourceBitmap, $destRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)




        $stream = New-Object System.IO.MemoryStream



        try {




            # El ICO del Explorador queda fijado en guinda Tlalpowa.
            # La misma decisión cromática se aplica al icono vivo GLFW; no se
            # delega al tema del sistema para impedir regresiones a gris.
            for ($yy = 0; $yy -lt $bitmap.Height; $yy++) {
                for ($xx = 0; $xx -lt $bitmap.Width; $xx++) {
                    $px = $bitmap.GetPixel($xx, $yy)
                    if ($px.A -gt 0) {
                        $bitmap.SetPixel($xx, $yy, [System.Drawing.Color]::FromArgb($px.A, 159, 34, 65))
                    }
                }
            }
            $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
            return $stream.ToArray()
        } finally {



            $stream.Dispose()
        }
    } finally {

        $graphics.Dispose()



        $bitmap.Dispose()
    }
}

# Write-IcoFile: serializa el contenedor ICO.
function Write-IcoFile([array] $Entries, [string] $Path) {



    $parent = [System.IO.Path]::GetDirectoryName($Path)

    if ($parent -and -not [System.IO.Directory]::Exists($parent)) {
        [System.IO.Directory]::CreateDirectory($parent) | Out-Null
    }




    $tmp = $Path + '.tmp.' + [System.Diagnostics.Process]::GetCurrentProcess().Id + '.ico'
    if ([System.IO.File]::Exists($tmp)) { [System.IO.File]::Delete($tmp) }

    $fs = [System.IO.File]::Open($tmp, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)




    $writer = New-Object System.IO.BinaryWriter $fs



    try {
        $writer.Write([UInt16]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]$Entries.Count)




        $offset = 6 + ($Entries.Count * 16)

        foreach ($entry in $Entries) {
            $writer.Write((Get-IcoDimensionByte $entry.Size))
            $writer.Write((Get-IcoDimensionByte $entry.Size))



            $writer.Write([byte]0)
            $writer.Write([byte]0)
            $writer.Write([UInt16]1)

            $writer.Write([UInt16]32)



            $writer.Write([UInt32]$entry.Bytes.Length)
            $writer.Write([UInt32]$offset)
            $offset += $entry.Bytes.Length
        }



        foreach ($entry in $Entries) {

            $writer.Write([byte[]]$entry.Bytes)
        }
    } finally {



        $writer.Dispose()
        $fs.Dispose()
    }


    if ([System.IO.File]::Exists($Path)) {



        [System.IO.File]::Delete($Path)
    }
    [System.IO.File]::Move($tmp, $Path)
}




$inputFull = [System.IO.Path]::GetFullPath($InputPng)

$outputFull = [System.IO.Path]::GetFullPath($OutputIco)
if (-not [System.IO.File]::Exists($inputFull)) {
    throw "No existe el PNG fuente: $inputFull"



}

Add-Type -AssemblyName System.Drawing

$image = $null

$bitmap = $null



try {
    $image = [System.Drawing.Image]::FromFile($inputFull)
    if ($image.Width -lt 1 -or $image.Height -lt 1) { throw 'PNG sin dimensiones validas.' }




    $bitmap = New-Object System.Drawing.Bitmap $image.Width, $image.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)



    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver



        $graphics.DrawImage($image, 0, 0, $image.Width, $image.Height)
    } finally {
        $graphics.Dispose()

    }




    $uniqueSizes = @($Sizes | Where-Object { $_ -ge 16 -and $_ -le 256 } | Sort-Object -Unique)
    if (-not $uniqueSizes -or $uniqueSizes.Count -eq 0) { throw 'No hay tamanos validos para el ICO.' }

    $entries = @()
    foreach ($size in $uniqueSizes) {



        $bytes = New-ResampledSquarePngBytes $bitmap $size

        $entries += [PSCustomObject]@{ Size = [int]$size; Bytes = $bytes }
    }




    Write-IcoFile $entries $outputFull



    Write-Info ("Convertido: {0} -> {1} ({2} tamanos)" -f $inputFull, $outputFull, $entries.Count)
} finally {
    if ($bitmap -ne $null) { $bitmap.Dispose() }

    if ($image -ne $null) { $image.Dispose() }



}
