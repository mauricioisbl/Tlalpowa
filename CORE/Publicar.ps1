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
        "Suite",
        "Todo"
    )]
    [string]$Target = "Todo",

    [switch]$NoCompile,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2.0

$suiteRoot = Split-Path -Parent $PSScriptRoot
$technicalRoot = Join-Path $env:LOCALAPPDATA "MiausoftSuite\Tecnico"
$publicationRoot = Join-Path $technicalRoot "Publicacion"
$logRoot = Join-Path $technicalRoot "Logs"
$logPath = Join-Path $logRoot ("Publicar-{0}-{1}.log" -f $Target, (Get-Date -Format "yyyyMMdd-HHmmss"))
$repositoriesPath = Join-Path $PSScriptRoot "Repositorios.json"

$suiteExecutables = @(
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

$suiteSelection = @{
    Ilnamiki = @{
        Directory = "ILNAMIKI"
        Files = @("Ilnamiki.exe")
    }
    Biblioteca = @{
        Directory = "MIAUSOFTOOLS\Biblioteca"
        Files = @("Biblioteca.exe")
    }
    ConvertidorCompleto = @{
        Directory = "MIAUSOFTOOLS\Convertidor_Completo"
        Files = @("Miausoft_Convertidor_Completo.exe")
    }
    ConvertidorCapitulos = @{
        Directory = "MIAUSOFTOOLS\Convertidor_Por_Capitulos"
        Files = @("Miausoft_Convertidor_Por_Capitulos.exe")
    }
    FusionadorDivisor = @{
        Directory = "MIAUSOFTOOLS\Fusionador_Divisor"
        Files = @("Miausoft_Fusionador_Divisor.exe")
    }
    Reemplazador = @{
        Directory = "MIAUSOFTOOLS\Reemplazador_Caracteres"
        Files = @("Miausoft_Reemplazador_Caracteres.exe")
    }
    Organizador = @{
        Directory = "MIAUSOFTOOLS\Organizador_Biblioteca"
        Files = @("Organizador.exe")
    }
    Installer = @{
        Directory = "MIAUSOFTOOLS\Installer"
        Files = @("MiausoftSuite_Installer.exe")
    }
    MiausoftTools = @{
        Directory = "MIAUSOFTOOLS"
        Files = @(
            "Miausoft_Convertidor_Completo.exe",
            "Miausoft_Convertidor_Por_Capitulos.exe",
            "Miausoft_Fusionador_Divisor.exe",
            "Miausoft_Reemplazador_Caracteres.exe",
            "Organizador.exe",
            "MiausoftSuite_Installer.exe"
        )
    }
}

function Write-Log {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host $Message
    Add-Content -LiteralPath $logPath -Encoding UTF8 -Value $Message
}

function Resolve-Git {
    $command = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $(if ($command.Source) { $command.Source } else { $command.Path })
    }
    $fallback = "C:\Program Files\Git\cmd\git.exe"
    if (Test-Path -LiteralPath $fallback -PathType Leaf) {
        return $fallback
    }
    throw "No se encontro git.exe."
}

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [switch]$AllowFailure,
        [switch]$Quiet
    )

    Write-Log ("> git " + ($Arguments -join " "))
    Push-Location -LiteralPath $Repository
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = & $script:GitExe @Arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
        Pop-Location
    }
    foreach ($line in $output) {
        $text = "  " + [string]$line
        if ($Quiet) {
            Add-Content -LiteralPath $logPath -Encoding UTF8 -Value $text
        } else {
            Write-Log $text
        }
    }
    if ($code -ne 0 -and -not $AllowFailure) {
        throw "Git termino con codigo ${code}: git $($Arguments -join ' ')"
    }
    return [pscustomobject]@{
        Code = [int]$code
        Output = @($output)
    }
}

function Assert-OwnedPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $resolvedRoot = [System.IO.Path]::GetFullPath($publicationRoot).TrimEnd("\") + "\"
    if (-not $resolvedPath.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "La ruta tecnica queda fuera del area de publicacion: $resolvedPath"
    }
}

function Ensure-Clone {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][pscustomobject]$Definition
    )

    $repository = Join-Path $publicationRoot $Name
    Assert-OwnedPath -Path $repository

    if (Test-Path -LiteralPath $repository -PathType Container) {
        $gitDirectory = Join-Path $repository ".git"
        if (-not (Test-Path -LiteralPath $gitDirectory)) {
            Remove-Item -LiteralPath $repository -Recurse -Force
        }
    }

    if (-not (Test-Path -LiteralPath (Join-Path $repository ".git"))) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $repository) | Out-Null
        Write-Log "Clonando $($Definition.url) en el area tecnica..."
        $previousErrorAction = $ErrorActionPreference
        try {
            $ErrorActionPreference = "Continue"
            $cloneOutput = & $script:GitExe -c core.longpaths=true clone --no-checkout $Definition.url $repository 2>&1
            $cloneCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $previousErrorAction
        }
        foreach ($line in $cloneOutput) {
            Write-Log ("  " + [string]$line)
        }
        if ($cloneCode -ne 0) {
            throw "No se pudo clonar $($Definition.url)."
        }
    }

    Invoke-Git -Repository $repository -Arguments @("config", "core.longpaths", "true") | Out-Null
    Invoke-Git -Repository $repository -Arguments @("config", "core.autocrlf", "false") | Out-Null
    $origin = Invoke-Git -Repository $repository -Arguments @("remote", "get-url", "origin")
    $originUrl = (($origin.Output | ForEach-Object { [string]$_ }) -join "").Trim()
    if ($originUrl -ne $Definition.url) {
        throw "El clon tecnico $Name apunta a '$originUrl', no a '$($Definition.url)'."
    }

    Invoke-Git -Repository $repository -Arguments @("fetch", "origin", $Definition.branch, "--prune") | Out-Null
    Invoke-Git -Repository $repository -Arguments @("checkout", "-B", $Definition.branch, "origin/$($Definition.branch)") | Out-Null
    Invoke-Git -Repository $repository -Arguments @("reset", "--hard", "origin/$($Definition.branch)") | Out-Null
    Invoke-Git -Repository $repository -Arguments @("clean", "-fdx") | Out-Null
    Invoke-Git -Repository $repository -Arguments @("config", "user.name", "Miausoft Publisher") | Out-Null
    Invoke-Git -Repository $repository -Arguments @("config", "user.email", "miausoft-publisher@users.noreply.github.com") | Out-Null
    return $repository
}

function Clear-Snapshot {
    param([Parameter(Mandatory = $true)][string]$Repository)

    Assert-OwnedPath -Path $Repository
    if (-not (Test-Path -LiteralPath (Join-Path $Repository ".git"))) {
        throw "Se rechazo limpiar una ruta que no es el clon tecnico esperado: $Repository"
    }
    Get-ChildItem -LiteralPath $Repository -Force |
        Where-Object { $_.Name -ne ".git" } |
        Remove-Item -Recurse -Force
}

function Copy-DirectoryMirror {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Repository,
        [string[]]$ExcludeDirectories = @(),
        [string[]]$ExcludeFiles = @()
    )

    $source = Join-Path $suiteRoot $RelativePath
    $destination = Join-Path $Repository $RelativePath
    if (-not (Test-Path -LiteralPath $source -PathType Container)) {
        throw "Falta el directorio publicable: $source"
    }
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    $arguments = @(
        $source,
        $destination,
        "/MIR",
        "/R:2",
        "/W:1",
        "/COPY:DAT",
        "/DCOPY:DAT",
        "/XJ",
        "/NFL",
        "/NDL",
        "/NJH",
        "/NJS",
        "/NP"
    )
    if ($ExcludeDirectories.Count -gt 0) {
        $arguments += "/XD"
        $arguments += $ExcludeDirectories
    }
    if ($ExcludeFiles.Count -gt 0) {
        $arguments += "/XF"
        $arguments += $ExcludeFiles
    }
    & robocopy.exe @arguments | Out-Null
    $code = $LASTEXITCODE
    if ($code -ge 8) {
        throw "Robocopy fallo con codigo $code al sincronizar $RelativePath."
    }
}

function Copy-RootFile {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Repository,
        [switch]$Optional
    )

    $source = Join-Path $suiteRoot $Name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        if ($Optional) {
            return
        }
        throw "Falta el archivo publicable: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $Repository $Name) -Force
}

function Copy-CommonRootFiles {
    param([Parameter(Mandatory = $true)][string]$Repository)
    Copy-RootFile -Name "Compilar.cmd" -Repository $Repository
    Copy-RootFile -Name "Publicar.cmd" -Repository $Repository
    Copy-RootFile -Name ".gitignore" -Repository $Repository
}

function Copy-Core {
    param([Parameter(Mandatory = $true)][string]$Repository)
    Copy-DirectoryMirror -RelativePath "CORE" -Repository $Repository `
        -ExcludeDirectories @("Runtime") `
        -ExcludeFiles @("dependencies.local.json", "*.user", "*.suo")
}

function New-TlalpowaManifest {
    param([Parameter(Mandatory = $true)][string]$Repository)

    $files = Get-ChildItem -LiteralPath $Repository -Recurse -File -Force |
        Where-Object {
            $_.FullName -notlike (Join-Path $Repository ".git\*") -and
            $_.Name -ne "tlalpowa_actualizacion_manifest.json"
        } |
        ForEach-Object {
            $relative = $_.FullName.Substring($Repository.Length).TrimStart("\").Replace("\", "/")
            [pscustomobject]@{
                path = $relative
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                size = [uint64]$_.Length
                data = $relative.StartsWith("TLALPOWA/Datos/", [System.StringComparison]::OrdinalIgnoreCase)
            }
        } |
        Sort-Object path

    $manifest = [ordered]@{
        schema = 1
        generated_utc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
        files = @($files)
    }
    $json = $manifest | ConvertTo-Json -Depth 5
    [System.IO.File]::WriteAllText(
        (Join-Path $Repository "tlalpowa_actualizacion_manifest.json"),
        $json + [Environment]::NewLine,
        (New-Object System.Text.UTF8Encoding($false))
    )
}

function Prepare-TlalpowaSnapshot {
    param([Parameter(Mandatory = $true)][string]$Repository)

    Clear-Snapshot -Repository $Repository
    Copy-CommonRootFiles -Repository $Repository
    Copy-Core -Repository $Repository
    Copy-DirectoryMirror -RelativePath "TLALPOWA" -Repository $Repository `
        -ExcludeDirectories @("Descargas", "Build", "out", ".vs", "__pycache__") `
        -ExcludeFiles @("*.obj", "*.pdb", "*.ilk", "*.user", "*.suo")
    Copy-RootFile -Name "Tlalpowa.exe" -Repository $Repository
    New-TlalpowaManifest -Repository $Repository
}

function Prepare-FullSuiteSnapshot {
    param([Parameter(Mandatory = $true)][string]$Repository)

    Clear-Snapshot -Repository $Repository
    Copy-CommonRootFiles -Repository $Repository
    Copy-Core -Repository $Repository
    Copy-DirectoryMirror -RelativePath "ILNAMIKI" -Repository $Repository
    Copy-DirectoryMirror -RelativePath "MIAUSOFTOOLS" -Repository $Repository `
        -ExcludeFiles @("*.user", "*.suo")
    Copy-DirectoryMirror -RelativePath "TLALPOWA" -Repository $Repository `
        -ExcludeDirectories @("Descargas", "Build", "out", ".vs", "__pycache__") `
        -ExcludeFiles @("*.obj", "*.pdb", "*.ilk", "*.user", "*.suo")
    foreach ($executable in $suiteExecutables) {
        Copy-RootFile -Name $executable -Repository $Repository
    }
}

function Prepare-SelectedSuiteSnapshot {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$Selection
    )

    if (-not (Test-Path -LiteralPath (Join-Path $Repository "CORE") -PathType Container)) {
        Write-Log "El remoto aun usa la estructura antigua; se migrara con una instantanea completa."
        Prepare-FullSuiteSnapshot -Repository $Repository
        return
    }

    Copy-CommonRootFiles -Repository $Repository
    Copy-Core -Repository $Repository
    $definition = $suiteSelection[$Selection]
    Copy-DirectoryMirror -RelativePath $definition.Directory -Repository $Repository `
        -ExcludeFiles @("*.user", "*.suo")
    foreach ($file in $definition.Files) {
        Copy-RootFile -Name $file -Repository $Repository
    }
}

function Assert-GitHubLimits {
    param([Parameter(Mandatory = $true)][string]$Repository)

    $tooLarge = Get-ChildItem -LiteralPath $Repository -Recurse -File -Force |
        Where-Object {
            $_.FullName -notlike (Join-Path $Repository ".git\*") -and
            $_.Length -ge 95MB
        }
    if ($tooLarge) {
        $names = ($tooLarge | ForEach-Object { $_.FullName }) -join [Environment]::NewLine
        throw "GitHub rechazaria archivos de 95 MiB o mas:`n$names"
    }
}

function Publish-Repository {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][pscustomobject]$Definition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    Assert-GitHubLimits -Repository $Repository
    Invoke-Git -Repository $Repository -Arguments @("add", "-A") -Quiet | Out-Null
    $status = Invoke-Git -Repository $Repository -Arguments @("status", "--short") -Quiet
    $changes = @($status.Output | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
    if ($changes.Count -eq 0) {
        Write-Log "[OK] $Name ya estaba actualizado; no se creo un commit vacio."
        return
    }

    if ($DryRun) {
        Write-Log "[SIMULACION] $Name tiene $($changes.Count) cambio(s); no se hara commit ni push."
        return
    }

    Invoke-Git -Repository $Repository -Arguments @("commit", "-m", $Message) | Out-Null
    Invoke-Git -Repository $Repository -Arguments @("push", "origin", $Definition.branch) | Out-Null
    Write-Log "[OK] $Name publicado en $($Definition.url)."
}

if (-not (Test-Path -LiteralPath $repositoriesPath -PathType Leaf)) {
    throw "Falta $repositoriesPath"
}
$configuration = Get-Content -LiteralPath $repositoriesPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($configuration.schema -ne 1) {
    throw "Version no compatible de Repositorios.json."
}

New-Item -ItemType Directory -Force -Path $publicationRoot, $logRoot | Out-Null
Set-Content -LiteralPath $logPath -Encoding UTF8 -Value @(
    "MiausoftSuite - publicacion central",
    "Inicio: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
    "Destino: $Target",
    "Simulacion: $DryRun",
    ""
)

$script:GitExe = Resolve-Git
$mutex = New-Object System.Threading.Mutex($false, "MiausoftSuite.Publisher")
$ownsMutex = $false
try {
    try {
        $ownsMutex = $mutex.WaitOne([TimeSpan]::FromMinutes(30))
    } catch [System.Threading.AbandonedMutexException] {
        $ownsMutex = $true
    }
    if (-not $ownsMutex) {
        throw "Hay otra publicacion de MiausoftSuite en curso."
    }

    if (-not $NoCompile) {
        $compileTarget = switch ($Target) {
            "Suite" { "Todo" }
            "Todo" { "Todo" }
            default { $Target }
        }
        Write-Log "Compilando $compileTarget antes de publicar..."
        & (Join-Path $PSScriptRoot "Compilar.ps1") -Target $compileTarget
        if (-not $?) {
            throw "La compilacion previa no termino correctamente."
        }
    }

    if ($Target -in @("Tlalpowa", "Todo")) {
        $definition = $configuration.repositories.Tlalpowa
        $repository = Ensure-Clone -Name "Tlalpowa" -Definition $definition
        Prepare-TlalpowaSnapshot -Repository $repository
        Publish-Repository -Repository $repository -Name "Tlalpowa" -Definition $definition `
            -Message ("Publicar Tlalpowa {0}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    }

    if ($Target -ne "Tlalpowa") {
        $definition = $configuration.repositories.MiausoftSuite
        $repository = Ensure-Clone -Name "MiausoftSuite" -Definition $definition
        if ($Target -in @("Suite", "Todo")) {
            Prepare-FullSuiteSnapshot -Repository $repository
        } else {
            Prepare-SelectedSuiteSnapshot -Repository $repository -Selection $Target
        }
        Publish-Repository -Repository $repository -Name "MiausoftSuite" -Definition $definition `
            -Message ("Publicar {0} {1}" -f $Target, (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    }

    Write-Host ""
    Write-Host $(if ($DryRun) { "Simulacion completada." } else { "Publicacion completada." }) -ForegroundColor Green
    Write-Host "Registro: $logPath"
} finally {
    if ($ownsMutex) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
