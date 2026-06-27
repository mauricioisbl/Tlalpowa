[CmdletBinding()]
param(
    [ValidateSet(
        "Tlalpowa",
        "Ilnamiki",
        "Organizador",
        "Todo"
    )]
    [string]$Target = "Todo",

    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",

    [switch]$Clean
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2.0

$suiteRoot = Split-Path -Parent $PSScriptRoot
$technicalRoot = Join-Path $env:LOCALAPPDATA "MiausoftSuite\Tecnico"
$buildRoot = Join-Path $technicalRoot ("Build\{0}-{1}" -f $Target, $Configuration)
$logRoot = Join-Path $technicalRoot "Logs"
$logPath = Join-Path $logRoot ("Compilar-{0}-{1}-{2}.log" -f $Target, $Configuration, (Get-Date -Format "yyyyMMdd-HHmmss"))

$targetInfo = @{
    Tlalpowa = @{
        CMake = @("Tlalpowa")
        Outputs = @("Tlalpowa.exe")
        Tlalpowa = $true
        Apps = $false
    }
    Ilnamiki = @{
        CMake = @("Ilnamiki")
        Outputs = @("Ilnamiki.exe")
        Tlalpowa = $false
        Apps = $true
    }
    Organizador = @{
        CMake = @("Organizador")
        Outputs = @("Organizador.exe")
        Tlalpowa = $false
        Apps = $true
    }
    Todo = @{
        CMake = @("Tlalpowa", "Ilnamiki", "Organizador")
        Outputs = @(
            "Tlalpowa.exe",
            "Ilnamiki.exe",
            "Organizador.exe"
        )
        Tlalpowa = $true
        Apps = $true
    }
}

function Resolve-Executable {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [string[]]$Fallbacks = @()
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $(if ($command.Source) { $command.Source } else { $command.Path })
    }
    foreach ($candidate in $Fallbacks) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return $candidate
        }
    }
    throw "No se encontro $Name."
}

function Invoke-Logged {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $printable = "$FilePath " + ($Arguments -join " ")
    Add-Content -LiteralPath $logPath -Encoding UTF8 -Value ("> " + $printable)
    & $FilePath @Arguments 2>&1 | Tee-Object -FilePath $logPath -Append
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        throw "Fallo el comando con codigo ${code}: $printable"
    }
}


function Convert-PngToMiausoftIco {
    param(
        [Parameter(Mandatory = $true)][string]$PngPath,
        [Parameter(Mandatory = $true)][string]$IcoPath
    )
    Add-Type -AssemblyName System.Drawing
    $sizes = @(16, 24, 32, 48, 64, 128, 256)
    $source = [System.Drawing.Image]::FromFile($PngPath)
    try {
        $images = New-Object 'System.Collections.Generic.List[byte[]]'
        foreach ($size in $sizes) {
            $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            try {
                $g = [System.Drawing.Graphics]::FromImage($bmp)
                try {
                    $g.Clear([System.Drawing.Color]::Transparent)
                    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                    $scale = [Math]::Min($size / $source.Width, $size / $source.Height)
                    $w = [Math]::Max(1, [int][Math]::Round($source.Width * $scale))
                    $h = [Math]::Max(1, [int][Math]::Round($source.Height * $scale))
                    $x = [int](($size - $w) / 2)
                    $y = [int](($size - $h) / 2)
                    $g.DrawImage($source, $x, $y, $w, $h)
                } finally { $g.Dispose() }
                $ms = New-Object System.IO.MemoryStream
                try {
                    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
                    $images.Add($ms.ToArray())
                } finally { $ms.Dispose() }
            } finally { $bmp.Dispose() }
        }
        $dir = Split-Path -Parent $IcoPath
        if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
        $fs = [System.IO.File]::Open($IcoPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
        try {
            $bw = New-Object System.IO.BinaryWriter($fs)
            try {
                $bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$sizes.Count)
                $offset = 6 + 16 * $sizes.Count
                for ($i = 0; $i -lt $sizes.Count; ++$i) {
                    $size = $sizes[$i]; $bytes = $images[$i]
                    $dim = if ($size -ge 256) { 0 } else { $size }
                    $bw.Write([byte]$dim)
                    $bw.Write([byte]$dim)
                    $bw.Write([byte]0); $bw.Write([byte]0)
                    $bw.Write([uint16]1); $bw.Write([uint16]32)
                    $bw.Write([uint32]$bytes.Length); $bw.Write([uint32]$offset)
                    $offset += $bytes.Length
                }
                foreach ($bytes in $images) { $bw.Write($bytes) }
            } finally { $bw.Dispose() }
        } finally { $fs.Dispose() }
    } finally { $source.Dispose() }
}


function Remove-IlnamikiWebResidue {
    param([Parameter(Mandatory = $true)][string]$Root)
    $ilnamiki = Join-Path $Root "ilnamiki"
    if (-not (Test-Path -LiteralPath $ilnamiki -PathType Container)) { return }
    $legacy = @("index.html", "ilnamiki.css", "ilnamiki.js", "ilnamiki.sql", "Ilnamiki.cpp", "LICENCE.md", "home.png", "pomodoro.png", "settings.png", "user.png")
    foreach ($name in $legacy) {
        $path = Join-Path $ilnamiki $name
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            Remove-Item -LiteralPath $path -Force
            Add-Content -LiteralPath $logPath -Encoding UTF8 -Value ("Residuo web Ilnamiki eliminado: {0}" -f $path)
        }
    }
}

function Ensure-MiausoftFolderIcons {
    param([Parameter(Mandatory = $true)][string]$Root)
    $skip = @('.git', 'Tecnico', 'Build', '.vs', '__pycache__')
    $dirs = @(Get-Item -LiteralPath $Root)
    $dirs += Get-ChildItem -LiteralPath $Root -Directory -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $skip -notcontains $_.Name }
    foreach ($dir in $dirs) {
        $folderName = $dir.Name
        if ([string]::IsNullOrWhiteSpace($folderName)) { continue }
        $png = Get-ChildItem -LiteralPath $dir.FullName -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Extension -ieq '.png' -and
                ([System.IO.Path]::GetFileNameWithoutExtension($_.Name) -ieq $folderName)
            } |
            Sort-Object LastWriteTimeUtc -Descending |
            Select-Object -First 1
        if (-not $png) { continue }
        $icoPath = Join-Path $dir.FullName ($folderName + '.ico')
        $needsIcon = $true
        if (Test-Path -LiteralPath $icoPath -PathType Leaf) {
            $needsIcon = ((Get-Item -LiteralPath $icoPath).LastWriteTimeUtc -lt $png.LastWriteTimeUtc)
        }
        if ($needsIcon) {
            Convert-PngToMiausoftIco -PngPath $png.FullName -IcoPath $icoPath
            Add-Content -LiteralPath $logPath -Encoding UTF8 -Value ("Icono generado: {0}" -f $icoPath)
        }
    }
}

$mutex = New-Object System.Threading.Mutex($false, "MiausoftSuite.Compiler")
$ownsMutex = $false
try {
    try {
        $ownsMutex = $mutex.WaitOne([TimeSpan]::FromMinutes(30))
    } catch [System.Threading.AbandonedMutexException] {
        $ownsMutex = $true
    }
    if (-not $ownsMutex) {
        throw "Hay otra compilacion de MiausoftSuite en curso."
    }

    if ($Clean -and (Test-Path -LiteralPath $buildRoot -PathType Container)) {
        $resolvedBuild = [System.IO.Path]::GetFullPath($buildRoot)
        $resolvedTechnical = [System.IO.Path]::GetFullPath($technicalRoot).TrimEnd("\") + "\"
        if (-not $resolvedBuild.StartsWith($resolvedTechnical, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Ruta de limpieza fuera del area tecnica: $resolvedBuild"
        }
        Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $buildRoot, $logRoot | Out-Null
    Set-Content -LiteralPath $logPath -Encoding UTF8 -Value @(
        "MiausoftSuite - compilacion central",
        "Inicio: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
        "Destino: $Target",
        "Configuracion: $Configuration",
        "Build: $buildRoot",
        ""
    )

    Remove-IlnamikiWebResidue -Root $suiteRoot
    Ensure-MiausoftFolderIcons -Root $suiteRoot

    $cmakeExe = Resolve-Executable -Name "cmake.exe" -Fallbacks @(
        "C:\Program Files\CMake\bin\cmake.exe"
    )
    $vswhere = Resolve-Executable -Name "vswhere.exe" -Fallbacks @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    )
    $vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsRoot) {
        throw "No se encontro una instalacion de Visual Studio con C++."
    }

    $vsDevCmd = Join-Path $vsRoot "Common7\Tools\VsDevCmd.bat"
    $environment = & cmd.exe /s /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && set"
    foreach ($line in $environment) {
        if ($line -match "^([^=]+)=(.*)$") {
            Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
        }
    }

    $generatorArgs = @("-G", "Ninja")
    $bundledNinja = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    if (Test-Path -LiteralPath $bundledNinja -PathType Leaf) {
        $generatorArgs += "-DCMAKE_MAKE_PROGRAM=$bundledNinja"
    }

    $info = $targetInfo[$Target]
    foreach ($name in $info.Outputs) {
        $path = Join-Path $suiteRoot $name
        Get-Process -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -eq $path } |
            Stop-Process -Force
    }

    $configureArgs = @(
        "-S", $PSScriptRoot,
        "-B", $buildRoot,
        "--fresh",
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DMIAUSOFT_BUILD_TLALPOWA=$(if ($info.Tlalpowa) { 'ON' } else { 'OFF' })",
        "-DMIAUSOFT_BUILD_APPS=$(if ($info.Apps) { 'ON' } else { 'OFF' })",
        "-DMIAUSOFT_SELECTED_TARGET=$Target"
    ) + $generatorArgs
    Invoke-Logged -FilePath $cmakeExe -Arguments $configureArgs

    $processorCount = [Math]::Max(1, [Environment]::ProcessorCount)
    $jobs = [Math]::Min(4, $processorCount)
    if ($info.Tlalpowa) {
        $jobs = [Math]::Min(2, $jobs)
    }
    if ($env:MIAUSOFT_BUILD_JOBS -match "^[1-9][0-9]*$") {
        $jobs = [int]$env:MIAUSOFT_BUILD_JOBS
    }

    $buildArgs = @("--build", $buildRoot, "--parallel", "$jobs", "--target") + $info.CMake
    Invoke-Logged -FilePath $cmakeExe -Arguments $buildArgs

    foreach ($name in $info.Outputs) {
        $path = Join-Path $suiteRoot $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            $found = Get-ChildItem -LiteralPath $buildRoot -Filter $name -File -Recurse -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($found) {
                Copy-Item -LiteralPath $found.FullName -Destination $path -Force
            }
        }
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "No se genero $path"
        }
    }

    Write-Host ""
    Write-Host "Compilacion completada: $Target [$Configuration]" -ForegroundColor Green
    Write-Host "Registro: $logPath"
} finally {
    if ($ownsMutex) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
