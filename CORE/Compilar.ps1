[CmdletBinding()]
param(
    [ValidateSet(
        "Tlalpowa",
        "Ilnamiki",
        "Biblioteca",
        "ConvertidorCompleto",
        "ConvertidorCapitulos",
        "FusionadorDivisor",
        "Reemplazador",
        "Organizador",
        "Installer",
        "MiausoftTools",
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
    Biblioteca = @{
        CMake = @("Biblioteca")
        Outputs = @("Biblioteca.exe")
        Tlalpowa = $false
        Apps = $true
    }
    ConvertidorCompleto = @{
        CMake = @("Convertidor_Completo")
        Outputs = @("Miausoft_Convertidor_Completo.exe")
        Tlalpowa = $false
        Apps = $true
    }
    ConvertidorCapitulos = @{
        CMake = @("Convertidor_Por_Capitulos")
        Outputs = @("Miausoft_Convertidor_Por_Capitulos.exe")
        Tlalpowa = $false
        Apps = $true
    }
    FusionadorDivisor = @{
        CMake = @("Fusionador_Divisor")
        Outputs = @("Miausoft_Fusionador_Divisor.exe")
        Tlalpowa = $false
        Apps = $true
    }
    Reemplazador = @{
        CMake = @("Reemplazador_Caracteres")
        Outputs = @("Miausoft_Reemplazador_Caracteres.exe")
        Tlalpowa = $false
        Apps = $true
    }
    Organizador = @{
        CMake = @("Organizador")
        Outputs = @("Organizador.exe")
        Tlalpowa = $false
        Apps = $true
    }
    Installer = @{
        CMake = @("MiausoftSuite_Installer")
        Outputs = @("MiausoftSuite_Installer.exe")
        Tlalpowa = $false
        Apps = $true
    }
    MiausoftTools = @{
        CMake = @("MiausoftTools")
        Outputs = @(
            "Miausoft_Convertidor_Completo.exe",
            "Miausoft_Convertidor_Por_Capitulos.exe",
            "Miausoft_Fusionador_Divisor.exe",
            "Miausoft_Reemplazador_Caracteres.exe",
            "Organizador.exe",
            "MiausoftSuite_Installer.exe"
        )
        Tlalpowa = $false
        Apps = $true
    }
    Todo = @{
        CMake = @("Tlalpowa", "Ilnamiki", "Biblioteca", "MiausoftTools")
        Outputs = @(
            "Tlalpowa.exe",
            "Ilnamiki.exe",
            "Biblioteca.exe",
            "Miausoft_Convertidor_Completo.exe",
            "Miausoft_Convertidor_Por_Capitulos.exe",
            "Miausoft_Fusionador_Divisor.exe",
            "Miausoft_Reemplazador_Caracteres.exe",
            "Organizador.exe",
            "MiausoftSuite_Installer.exe"
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
        "-DMIAUSOFT_BUILD_APPS=$(if ($info.Apps) { 'ON' } else { 'OFF' })"
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
