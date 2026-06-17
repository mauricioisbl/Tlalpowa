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
    [switch]$DryRun,
    [switch]$Login
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2.0

$suiteRoot = Split-Path -Parent $PSScriptRoot
$technicalRoot = Join-Path $env:LOCALAPPDATA "MiausoftSuite\Tecnico"
$publicationRoot = Join-Path $technicalRoot "Publicacion"
$logRoot = Join-Path $technicalRoot "Logs"
$logPath = Join-Path $logRoot ("Publicar-{0}-{1}.log" -f $Target, (Get-Date -Format "yyyyMMdd-HHmmss"))
$listPath = Join-Path $logRoot ("Publicar-{0}-{1}-archivos-actualizados.txt" -f $Target, (Get-Date -Format "yyyyMMdd-HHmmss"))
$repositoriesPath = Join-Path $PSScriptRoot "Repositorios.json"
$externalDriveRoot = "D:\MiausoftSuite"

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

function Convert-ByteCountToKilobytes {
    param([Parameter(Mandatory = $true)][uint64]$ByteCount)
    if ($ByteCount -eq 0) { return [uint64]0 }
    return [uint64][Math]::Ceiling(([double]$ByteCount) / 1024.0)
}

function Get-FileKilobytesOrNull {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }
    $item = Get-Item -LiteralPath $Path -Force
    return (Convert-ByteCountToKilobytes -ByteCount ([uint64]$item.Length))
}

function Normalize-RelativePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return $Path.TrimStart([char[]]"\/").Replace("\", "/")
}

function Join-RootRelative {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )
    return (Join-Path $Root ((Normalize-RelativePath -Path $RelativePath).Replace("/", "\")))
}

function New-ChangeBucket {
    $bucket = New-Object 'System.Collections.Generic.List[object]'
    if ($null -eq $bucket) {
        throw "No se pudo crear la lista interna de cambios del publicador."
    }
    Write-Output -NoEnumerate $bucket
}

function New-PathBucket {
    $bucket = New-Object 'System.Collections.Generic.List[string]'
    if ($null -eq $bucket) {
        throw "No se pudo crear la lista interna de rutas del publicador."
    }
    Write-Output -NoEnumerate $bucket
}

function Assert-Bucket {
    param(
        $Value,
        [Parameter(Mandatory = $true)][string]$Name
    )
    if ($null -eq $Value) {
        throw "$Name es nulo; el publicador no puede continuar con una lista de trabajo vacia."
    }
    if (-not ($Value.PSObject.Methods.Name -contains "Add") -or
        -not ($Value.PSObject.Methods.Name -contains "ToArray")) {
        throw "$Name no es una lista mutable valida para el publicador."
    }
}

function Add-UniquePath {
    param(
        [Parameter(Mandatory = $true)]$List,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )
    $normalized = Normalize-RelativePath -Path $RelativePath
    if (-not $List.Contains($normalized)) {
        $List.Add($normalized) | Out-Null
    }
}

function Add-Change {
    param(
        [Parameter(Mandatory = $true)]$Changes,
        [Parameter(Mandatory = $true)][string]$Destination,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Nullable[uint64]]$PreviousKB,
        [Parameter(Mandatory = $true)][uint64]$CurrentKB,
        [string]$Kind = "archivo"
    )
    $Changes.Add([pscustomobject]@{
        Destination = $Destination
        RelativePath = (Normalize-RelativePath -Path $RelativePath)
        PreviousKB = $PreviousKB
        CurrentKB = $CurrentKB
        Kind = $Kind
    }) | Out-Null
}

function Add-Deletion {
    param(
        [Parameter(Mandatory = $true)]$Changes,
        [Parameter(Mandatory = $true)][string]$Destination,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Nullable[uint64]]$PreviousKB,
        [string]$Kind = "eliminado"
    )
    $Changes.Add([pscustomobject]@{
        Destination = $Destination
        RelativePath = (Normalize-RelativePath -Path $RelativePath)
        PreviousKB = $PreviousKB
        CurrentKB = $null
        Kind = $Kind
    }) | Out-Null
}

function Add-PathList {
    param(
        [Parameter(Mandatory = $true)]$Destination,
        [Parameter(Mandatory = $true)]$Paths
    )
    foreach ($path in @($Paths)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$path)) {
            Add-UniquePath -List $Destination -RelativePath ([string]$path)
        }
    }
}

function Add-ChangeList {
    param(
        [Parameter(Mandatory = $true)]$Destination,
        [Parameter(Mandatory = $true)]$Items
    )
    foreach ($item in @($Items)) {
        if ($null -ne $item) {
            $Destination.Add($item) | Out-Null
        }
    }
}

function Format-KBValue {
    param($Value)
    if ($null -eq $Value) { return "ausente" }
    return ("{0} KB" -f $Value)
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

function Invoke-GitCredentialManager {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [switch]$AllowFailure
    )

    Write-Log ("> git credential-manager " + ($Arguments -join " "))
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = & $script:GitExe credential-manager @Arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }
    foreach ($line in @($output)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$line)) {
            Write-Log ("  " + [string]$line)
        }
    }
    if ($code -ne 0 -and -not $AllowFailure) {
        throw "Git Credential Manager termino con codigo ${code}: git credential-manager $($Arguments -join ' ')"
    }
    return [pscustomobject]@{
        Code = [int]$code
        Output = @($output)
    }
}

function Get-GitHubCredentialAccounts {
    $result = Invoke-GitCredentialManager -Arguments @("github", "list") -AllowFailure
    if ($result.Code -ne 0) { return @() }
    return @($result.Output |
        ForEach-Object { ([string]$_).Trim() } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and $_ -notmatch "^(Description|Usage|Options|Commands):" })
}

function Ensure-GitHubAuthentication {
    param([switch]$InteractiveLogin)

    Invoke-GitCredentialManager -Arguments @("configure") -AllowFailure | Out-Null
    $accounts = @(Get-GitHubCredentialAccounts)
    if ($accounts.Count -gt 0 -and -not $InteractiveLogin) {
        Write-Log ("[OK] GitHub autenticado en Git Credential Manager como: {0}" -f ($accounts -join ", "))
        return
    }

    if ($accounts.Count -gt 0) {
        Write-Log ("Cuenta(s) GitHub detectada(s): {0}" -f ($accounts -join ", "))
    } else {
        Write-Log "No hay cuentas GitHub guardadas para Git. Abrire el inicio de sesion de Git Credential Manager."
    }

    Invoke-GitCredentialManager -Arguments @("github", "login") | Out-Null
    $accounts = @(Get-GitHubCredentialAccounts)
    if ($accounts.Count -eq 0) {
        throw "Git Credential Manager no registro ninguna cuenta GitHub. Inicia sesion con Publicar.cmd /iniciar-sesion y acepta la autorizacion en el navegador."
    }
    Write-Log ("[OK] GitHub autenticado para Git como: {0}" -f ($accounts -join ", "))
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
        $output = & $script:GitExe -c core.longpaths=true -c gc.auto=0 -c maintenance.auto=false @Arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
        Pop-Location
    }
    if (-not $Quiet -or $code -ne 0) {
        foreach ($line in $output) {
            $text = "  " + [string]$line
            if ($Quiet) {
                Add-Content -LiteralPath $logPath -Encoding UTF8 -Value $text
            } else {
                Write-Log $text
            }
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
            $cloneOutput = & $script:GitExe -c core.longpaths=true -c gc.auto=0 -c maintenance.auto=false clone --no-checkout $Definition.url $repository 2>&1
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

    Invoke-Git -Repository $repository -Arguments @("fetch", "origin", $Definition.branch, "--prune") -Quiet | Out-Null
    Invoke-Git -Repository $repository -Arguments @("checkout", "-B", $Definition.branch, "origin/$($Definition.branch)") -Quiet | Out-Null
    Invoke-Git -Repository $repository -Arguments @("reset", "--hard", "origin/$($Definition.branch)") -Quiet | Out-Null
    Invoke-Git -Repository $repository -Arguments @("clean", "-fdx") -Quiet | Out-Null
    Invoke-Git -Repository $repository -Arguments @("config", "user.name", "Miausoft Publisher") | Out-Null
    Invoke-Git -Repository $repository -Arguments @("config", "user.email", "miausoft-publisher@users.noreply.github.com") | Out-Null
    return $repository
}

function Test-ExcludedRelativePath {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [string[]]$ExcludeDirectories = @(),
        [string[]]$ExcludeFiles = @()
    )

    $relative = Normalize-RelativePath -Path $RelativePath
    $leaf = Split-Path $relative -Leaf

    foreach ($directory in @($ExcludeDirectories)) {
        if ([string]::IsNullOrWhiteSpace($directory)) { continue }
        $normalizedDirectory = (Normalize-RelativePath -Path $directory).TrimEnd("/")
        if ($relative.Equals($normalizedDirectory, [System.StringComparison]::OrdinalIgnoreCase) -or
            $relative.StartsWith($normalizedDirectory + "/", [System.StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }

    foreach ($pattern in @($ExcludeFiles)) {
        if ([string]::IsNullOrWhiteSpace($pattern)) { continue }
        if ($leaf -like $pattern) { return $true }
    }

    return $false
}

function Copy-SourceFileByKilobytes {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$DestinationRoot,
        [Parameter(Mandatory = $true)][string]$DestinationName,
        [Parameter(Mandatory = $true)]$Changes,
        [Parameter(Mandatory = $true)]$SourcePaths,
        [switch]$Optional
    )

    $normalized = Normalize-RelativePath -Path $RelativePath
    $source = Join-RootRelative -Root $suiteRoot -RelativePath $normalized
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        if ($Optional) { return }
        throw "Falta el archivo publicable: $source"
    }

    Add-UniquePath -List $SourcePaths -RelativePath $normalized

    $destination = Join-RootRelative -Root $DestinationRoot -RelativePath $normalized
    $sourceKB = Get-FileKilobytesOrNull -Path $source
    $destinationKB = Get-FileKilobytesOrNull -Path $destination

    if ($null -eq $destinationKB -or $sourceKB -ne $destinationKB) {
        if (-not $DryRun) {
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
            [System.IO.File]::Copy($source, $destination, $true)
        }
        Add-Change -Changes $Changes -Destination $DestinationName -RelativePath $normalized `
            -PreviousKB $destinationKB -CurrentKB $sourceKB -Kind "archivo"
    }
}

function Copy-DirectoryByKilobytes {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$DestinationRoot,
        [Parameter(Mandatory = $true)][string]$DestinationName,
        [Parameter(Mandatory = $true)]$Changes,
        [Parameter(Mandatory = $true)]$SourcePaths,
        [string[]]$ExcludeDirectories = @(),
        [string[]]$ExcludeFiles = @()
    )

    $rootRelative = Normalize-RelativePath -Path $RelativePath
    $sourceRoot = Join-RootRelative -Root $suiteRoot -RelativePath $rootRelative
    if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) {
        throw "Falta el directorio publicable: $sourceRoot"
    }

    Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Force |
        ForEach-Object {
            $localRelative = $_.FullName.Substring($sourceRoot.Length).TrimStart([char[]]"\/").Replace("\", "/")
            $suiteRelative = (Normalize-RelativePath -Path ($rootRelative + "/" + $localRelative))
            if (-not (Test-ExcludedRelativePath -RelativePath $suiteRelative `
                    -ExcludeDirectories $ExcludeDirectories -ExcludeFiles $ExcludeFiles)) {
                Copy-SourceFileByKilobytes -RelativePath $suiteRelative -DestinationRoot $DestinationRoot `
                    -DestinationName $DestinationName -Changes $Changes -SourcePaths $SourcePaths
            }
        }
}

function Copy-CommonRootFiles {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$DestinationName,
        [Parameter(Mandatory = $true)]$Changes,
        [Parameter(Mandatory = $true)]$SourcePaths,
        [switch]$IncludePull
    )

    Copy-SourceFileByKilobytes -RelativePath "Compilar.cmd" -DestinationRoot $Repository `
        -DestinationName $DestinationName -Changes $Changes -SourcePaths $SourcePaths
    Copy-SourceFileByKilobytes -RelativePath "Publicar.cmd" -DestinationRoot $Repository `
        -DestinationName $DestinationName -Changes $Changes -SourcePaths $SourcePaths
    Copy-SourceFileByKilobytes -RelativePath ".gitignore" -DestinationRoot $Repository `
        -DestinationName $DestinationName -Changes $Changes -SourcePaths $SourcePaths -Optional
    if ($IncludePull) {
        Copy-SourceFileByKilobytes -RelativePath "JalarCambios.cmd" -DestinationRoot $Repository `
            -DestinationName $DestinationName -Changes $Changes -SourcePaths $SourcePaths -Optional
    }
}

function Test-RelativePathInScope {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [string[]]$ScopePrefixes = @()
    )

    $relative = Normalize-RelativePath -Path $RelativePath
    $scopes = @($ScopePrefixes | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
    if ($scopes.Count -eq 0) { return $true }

    foreach ($scopeItem in $scopes) {
        $scope = Normalize-RelativePath -Path ([string]$scopeItem)
        if ($scope.EndsWith("/", [System.StringComparison]::Ordinal)) {
            if ($relative.StartsWith($scope, [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
        } else {
            if ($relative.Equals($scope, [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
            if ($relative.StartsWith($scope + "/", [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
        }
    }
    return $false
}

function Remove-DestinationFilesMissingFromSource {
    param(
        [Parameter(Mandatory = $true)][string]$DestinationRoot,
        [Parameter(Mandatory = $true)][string]$DestinationName,
        [Parameter(Mandatory = $true)]$SourcePaths,
        [Parameter(Mandatory = $true)]$Changes,
        [string[]]$ScopePrefixes = @(),
        [string[]]$KeepFiles = @()
    )

    if (-not (Test-Path -LiteralPath $DestinationRoot -PathType Container)) { return }

    $sourceSet = New-Object 'System.Collections.Generic.HashSet[string]' -ArgumentList ([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($path in @($SourcePaths)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$path)) {
            [void]$sourceSet.Add((Normalize-RelativePath -Path ([string]$path)))
        }
    }
    foreach ($path in @($KeepFiles)) {
        if (-not [string]::IsNullOrWhiteSpace([string]$path)) {
            [void]$sourceSet.Add((Normalize-RelativePath -Path ([string]$path)))
        }
    }

    $rootFull = [System.IO.Path]::GetFullPath($DestinationRoot).TrimEnd([char[]]"\/")
    Get-ChildItem -LiteralPath $DestinationRoot -Recurse -File -Force |
        ForEach-Object {
            $full = [System.IO.Path]::GetFullPath($_.FullName)
            $relative = $full.Substring($rootFull.Length).TrimStart([char[]]"\/").Replace("\", "/")
            $relative = Normalize-RelativePath -Path $relative
            $insideGit = $relative.Equals(".git", [System.StringComparison]::OrdinalIgnoreCase) -or
                $relative.StartsWith(".git/", [System.StringComparison]::OrdinalIgnoreCase)
            if (-not $insideGit -and
                (Test-RelativePathInScope -RelativePath $relative -ScopePrefixes $ScopePrefixes) -and
                -not $sourceSet.Contains($relative)) {
                $previousKB = Get-FileKilobytesOrNull -Path $_.FullName
                if (-not $DryRun) {
                    Remove-Item -LiteralPath $_.FullName -Force
                }
                Add-Deletion -Changes $Changes -Destination $DestinationName -RelativePath $relative -PreviousKB $previousKB
            }
        }
}

function Copy-Core {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$DestinationName,
        [Parameter(Mandatory = $true)]$Changes,
        [Parameter(Mandatory = $true)]$SourcePaths
    )
    Copy-DirectoryByKilobytes -RelativePath "CORE" -DestinationRoot $Repository `
        -DestinationName $DestinationName -Changes $Changes -SourcePaths $SourcePaths `
        -ExcludeDirectories @("CORE/Runtime") `
        -ExcludeFiles @("dependencies.local.json", "*.user", "*.suo")
}

function Write-TextFileByKilobytes {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$DestinationName,
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)]$Changes,
        [string]$Kind = "generado"
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    $bytes = $encoding.GetBytes($Text)
    $newKB = Convert-ByteCountToKilobytes -ByteCount ([uint64]$bytes.Length)
    $oldKB = Get-FileKilobytesOrNull -Path $Path
    if ($null -eq $oldKB -or $newKB -ne $oldKB) {
        if (-not $DryRun) {
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
            [System.IO.File]::WriteAllText($Path, $Text, $encoding)
        }
        Add-Change -Changes $Changes -Destination $DestinationName -RelativePath $RelativePath `
            -PreviousKB $oldKB -CurrentKB $newKB -Kind $Kind
    }
}

function Read-TlalpowaManifestIndex {
    param([Parameter(Mandatory = $true)][string]$Path)

    $index = @{}
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        Write-Output -NoEnumerate $index
        return
    }

    try {
        $manifest = Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
        foreach ($file in @($manifest.files)) {
            try {
                if ($null -eq $file -or [string]::IsNullOrWhiteSpace([string]$file.path) -or
                    [string]::IsNullOrWhiteSpace([string]$file.sha256)) {
                    continue
                }
                $relative = Normalize-RelativePath -Path ([string]$file.path)
                $size = [uint64]$file.size
                $hasKB = $file.PSObject.Properties.Name -contains "kb"
                $kb = if ($hasKB -and $null -ne $file.kb) {
                    [uint64]$file.kb
                } else {
                    Convert-ByteCountToKilobytes -ByteCount $size
                }
                $index[$relative] = [pscustomobject]@{
                    Sha256 = ([string]$file.sha256).ToLowerInvariant()
                    Size = $size
                    KB = $kb
                }
            } catch {
                Write-Log "[AVISO] Entrada de manifiesto ignorada: $($_.Exception.Message)"
            }
        }
    } catch {
        Write-Log "[AVISO] No se pudo reutilizar el manifiesto anterior de Tlalpowa: $($_.Exception.Message)"
    }

    Write-Output -NoEnumerate $index
}

function New-TlalpowaManifest {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)]$Changes
    )

    $phase = "inicio"
    try {
        $manifestPath = Join-Path $Repository "tlalpowa_actualizacion_manifest.json"
        $phase = "leer manifiesto anterior"
        $previousFiles = Read-TlalpowaManifestIndex -Path $manifestPath
        $changedFiles = @{}
        $phase = "indexar cambios previos"
        foreach ($change in $Changes) {
            if ($null -eq $change) { continue }
            $relativeChangePath = [string]$change.RelativePath
            if ($null -ne $change.CurrentKB -and
                -not [string]::IsNullOrWhiteSpace($relativeChangePath)) {
                $changedFiles[(Normalize-RelativePath -Path $relativeChangePath)] = $true
            }
        }

        $phase = "enumerar archivos"
        $fileEntries = New-Object 'System.Collections.Generic.List[object]'
        $repositoryRoot = [System.IO.Path]::GetFullPath($Repository).TrimEnd([char[]]"\/")
        $rootFiles = @(
            "Tlalpowa.exe",
            "Compilar.cmd",
            "Publicar.cmd",
            "JalarCambios.cmd",
            ".gitignore"
        )
        foreach ($fileInfo in Get-ChildItem -LiteralPath $Repository -Recurse -File -Force) {
            $relative = [System.IO.Path]::GetFullPath($fileInfo.FullName).Substring($repositoryRoot.Length).TrimStart([char[]]"\/").Replace("\", "/")
            if ($relative.Equals(".git", [System.StringComparison]::OrdinalIgnoreCase) -or
                $relative.StartsWith(".git/", [System.StringComparison]::OrdinalIgnoreCase) -or
                $fileInfo.Name -eq "tlalpowa_actualizacion_manifest.json" -or
                $relative.StartsWith("TLALPOWA/Descargas/", [System.StringComparison]::OrdinalIgnoreCase) -or
                $relative.StartsWith("Build/", [System.StringComparison]::OrdinalIgnoreCase) -or
                $relative.IndexOf("/Build/", [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
                continue
            }
            $inScope = $rootFiles -contains $relative -or
                $relative.StartsWith("TLALPOWA/", [System.StringComparison]::OrdinalIgnoreCase) -or
                $relative.StartsWith("CORE/", [System.StringComparison]::OrdinalIgnoreCase)
            if (-not $inScope) { continue }

            try {
                $size = [uint64]$fileInfo.Length
                $kb = Convert-ByteCountToKilobytes -ByteCount $size
                $previous = $previousFiles[$relative]
                $canReuseHash = $null -ne $previous -and
                    -not $changedFiles.ContainsKey($relative) -and
                    $previous.Size -eq $size -and
                    $previous.KB -eq $kb -and
                    -not [string]::IsNullOrWhiteSpace($previous.Sha256)
                $sha256 = if ($canReuseHash) {
                    $previous.Sha256
                } else {
                    (Get-FileHash -LiteralPath $fileInfo.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                }
                $fileEntries.Add([pscustomobject]@{
                    path = $relative
                    sha256 = $sha256
                    size = $size
                    kb = $kb
                    data = $relative.StartsWith("TLALPOWA/Datos/", [System.StringComparison]::OrdinalIgnoreCase)
                }) | Out-Null
            } catch {
                throw "No se pudo preparar el manifiesto de Tlalpowa para '$relative': $($_.Exception.Message)"
            }
        }

        $phase = "ordenar manifiesto"
        $files = @($fileEntries.ToArray() | Sort-Object path)
        $phase = "convertir manifiesto a JSON"
        $manifest = [ordered]@{
            schema = 2
            criterion = "publish_only_when_kilobytes_changed"
            files = @($files)
        }
        $json = ($manifest | ConvertTo-Json -Depth 6) + [Environment]::NewLine
        $phase = "escribir manifiesto"
        Write-TextFileByKilobytes -Path $manifestPath `
            -RelativePath "tlalpowa_actualizacion_manifest.json" -DestinationName "Tlalpowa" `
            -Text $json -Changes $Changes -Kind "manifiesto"
    } catch {
        $changesType = if ($null -eq $Changes) { "null" } else { $Changes.GetType().FullName }
        $changesCount = if ($null -ne $Changes -and ($Changes.PSObject.Properties.Name -contains "Count")) { $Changes.Count } else { "desconocido" }
        throw "Fallo creando manifiesto de Tlalpowa en fase '$phase'. ChangesType=$changesType; ChangesCount=$changesCount; Error=$($_.Exception.Message)"
    }
}

function Prepare-TlalpowaSnapshot {
    param([Parameter(Mandatory = $true)][string]$Repository)

    $changes = New-ChangeBucket
    $sourcePaths = New-PathBucket
    Assert-Bucket -Value $changes -Name "Cambios Tlalpowa"
    Assert-Bucket -Value $sourcePaths -Name "Rutas Tlalpowa"

    Copy-CommonRootFiles -Repository $Repository -DestinationName "Tlalpowa" `
        -Changes $changes -SourcePaths $sourcePaths -IncludePull
    Copy-Core -Repository $Repository -DestinationName "Tlalpowa" `
        -Changes $changes -SourcePaths $sourcePaths
    Copy-DirectoryByKilobytes -RelativePath "TLALPOWA" -DestinationRoot $Repository `
        -DestinationName "Tlalpowa" -Changes $changes -SourcePaths $sourcePaths `
        -ExcludeDirectories @("TLALPOWA/Descargas", "TLALPOWA/Build", "TLALPOWA/out", "TLALPOWA/.vs", "TLALPOWA/__pycache__") `
        -ExcludeFiles @("*.obj", "*.pdb", "*.ilk", "*.user", "*.suo")
    Copy-SourceFileByKilobytes -RelativePath "Tlalpowa.exe" -DestinationRoot $Repository `
        -DestinationName "Tlalpowa" -Changes $changes -SourcePaths $sourcePaths
    New-TlalpowaManifest -Repository $Repository -Changes $changes
    Add-UniquePath -List $sourcePaths -RelativePath "tlalpowa_actualizacion_manifest.json"
    Remove-DestinationFilesMissingFromSource -DestinationRoot $Repository -DestinationName "Tlalpowa" `
        -SourcePaths $sourcePaths -Changes $changes `
        -ScopePrefixes @("CORE/", "TLALPOWA/", "Compilar.cmd", "Publicar.cmd", "JalarCambios.cmd", ".gitignore", "Tlalpowa.exe", "tlalpowa_actualizacion_manifest.json")

    return [pscustomobject]@{
        Changes = $changes.ToArray()
        SourcePaths = $sourcePaths.ToArray()
    }
}

function Prepare-FullSuiteSnapshot {
    param([Parameter(Mandatory = $true)][string]$Repository)

    $changes = New-ChangeBucket
    $sourcePaths = New-PathBucket
    Assert-Bucket -Value $changes -Name "Cambios MiausoftSuite"
    Assert-Bucket -Value $sourcePaths -Name "Rutas MiausoftSuite"
    $excludeDirectories = @(
        ".git",
        "Build",
        "build",
        "TLALPOWA/Descargas",
        "TLALPOWA/Build",
        "TLALPOWA/out",
        "TLALPOWA/.vs",
        "ILNAMIKI/Build",
        "MIAUSOFTOOLS/Build",
        "__pycache__"
    )
    $excludeFiles = @("*.obj", "*.pdb", "*.ilk", "*.user", "*.suo")

    Get-ChildItem -LiteralPath $suiteRoot -Recurse -File -Force |
        ForEach-Object {
            $relative = $_.FullName.Substring($suiteRoot.Length).TrimStart([char[]]"\/").Replace("\", "/")
            if (-not (Test-ExcludedRelativePath -RelativePath $relative `
                    -ExcludeDirectories $excludeDirectories -ExcludeFiles $excludeFiles)) {
                Copy-SourceFileByKilobytes -RelativePath $relative -DestinationRoot $Repository `
                    -DestinationName "MiausoftSuite" -Changes $changes -SourcePaths $sourcePaths
            }
        }
    Remove-DestinationFilesMissingFromSource -DestinationRoot $Repository -DestinationName "MiausoftSuite" `
        -SourcePaths $sourcePaths -Changes $changes

    return [pscustomobject]@{
        Changes = $changes.ToArray()
        SourcePaths = $sourcePaths.ToArray()
    }
}

function Prepare-SelectedSuiteSnapshot {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$Selection
    )

    $changes = New-ChangeBucket
    $sourcePaths = New-PathBucket
    Assert-Bucket -Value $changes -Name "Cambios de seleccion MiausoftSuite"
    Assert-Bucket -Value $sourcePaths -Name "Rutas de seleccion MiausoftSuite"

    Copy-CommonRootFiles -Repository $Repository -DestinationName "MiausoftSuite" `
        -Changes $changes -SourcePaths $sourcePaths -IncludePull
    Copy-Core -Repository $Repository -DestinationName "MiausoftSuite" `
        -Changes $changes -SourcePaths $sourcePaths
    $definition = $suiteSelection[$Selection]
    Copy-DirectoryByKilobytes -RelativePath $definition.Directory -DestinationRoot $Repository `
        -DestinationName "MiausoftSuite" -Changes $changes -SourcePaths $sourcePaths `
        -ExcludeFiles @("*.user", "*.suo")
    foreach ($file in $definition.Files) {
        Copy-SourceFileByKilobytes -RelativePath $file -DestinationRoot $Repository `
            -DestinationName "MiausoftSuite" -Changes $changes -SourcePaths $sourcePaths
    }

    return [pscustomobject]@{
        Changes = $changes.ToArray()
        SourcePaths = $sourcePaths.ToArray()
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

function Write-ChangeSummary {
    param(
        [Parameter(Mandatory = $true)][string]$Title,
        [Parameter(Mandatory = $true)]$Changes
    )

    $items = @($Changes)
    if ($items.Count -eq 0) {
        Write-Log "[OK] ${Title}: ningun archivo cambio de KB."
        return
    }

    Write-Log "[LISTA] ${Title}: $($items.Count) cambio(s) por KB o eliminacion."
    $sorted = @($items | Sort-Object Destination, RelativePath -Unique)
    $limit = 40
    foreach ($item in ($sorted | Select-Object -First $limit)) {
        $kind = if ($null -eq $item.CurrentKB) { "eliminado" } else { $item.Kind }
        Write-Log ("  {0}: {1} [{2}] ({3} -> {4})" -f $item.Destination, $item.RelativePath, $kind, (Format-KBValue $item.PreviousKB), (Format-KBValue $item.CurrentKB))
    }
    if ($sorted.Count -gt $limit) {
        Write-Log ("  ... {0} cambio(s) mas. El detalle completo queda en la lista final de archivos actualizados." -f ($sorted.Count - $limit))
    }
}

function Assert-GitHubPushAccess {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][pscustomobject]$Definition
    )

    Ensure-GitHubAuthentication
    $result = Invoke-Git -Repository $Repository `
        -Arguments @("push", "--dry-run", "--porcelain", "origin", "HEAD:$($Definition.branch)") `
        -AllowFailure -Quiet
    if ($result.Code -eq 0) {
        Write-Log "[OK] GitHub acepto la autenticacion para publicar $Name."
        return
    }

    $details = (@($result.Output) | ForEach-Object { [string]$_ }) -join [Environment]::NewLine
    throw @"
GitHub no acepto el acceso de escritura para $Name.
Repositorio: $($Definition.url)
Cuenta detectada por Git Credential Manager: $((@(Get-GitHubCredentialAccounts)) -join ", ")

Esto no depende de estar logueado en github.com dentro del navegador normal: Git necesita credenciales guardadas en Git Credential Manager.
Ejecuta: Publicar.cmd /iniciar-sesion

Detalle de Git:
$details
"@
}

function Publish-Repository {
    param(
        [Parameter(Mandatory = $true)][string]$Repository,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][pscustomobject]$Definition,
        [Parameter(Mandatory = $true)][string]$Message,
        [Parameter(Mandatory = $true)]$ChangedFiles
    )

    $paths = @($ChangedFiles | ForEach-Object { Normalize-RelativePath -Path ([string]$_.RelativePath) } | Sort-Object -Unique)
    if ($paths.Count -eq 0) {
        Write-Log "[OK] $Name ya estaba actualizado por KB; no se creo commit."
        return
    }

    if ($DryRun) {
        Write-Log "[SIMULACION] $Name tiene $($paths.Count) ruta(s) cambiada(s); no se hara git add, commit ni push."
        return
    }

    Assert-GitHubPushAccess -Repository $Repository -Name $Name -Definition $Definition
    Assert-GitHubLimits -Repository $Repository
    $step = 80
    for ($i = 0; $i -lt $paths.Count; $i += $step) {
        $end = [Math]::Min($i + $step - 1, $paths.Count - 1)
        $chunk = @($paths[$i..$end])
        Invoke-Git -Repository $Repository -Arguments (@("add", "--") + $chunk) -Quiet | Out-Null
    }

    $status = Invoke-Git -Repository $Repository -Arguments @("status", "--short") -Quiet
    $changes = @($status.Output | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
    if ($changes.Count -eq 0) {
        Write-Log "[OK] $Name tenia lista de KB cambiado, pero Git no encontro diferencias publicables."
        return
    }

    Invoke-Git -Repository $Repository -Arguments @("commit", "-m", $Message) | Out-Null
    Invoke-Git -Repository $Repository -Arguments @("push", "origin", $Definition.branch) | Out-Null
    Write-Log "[OK] $Name publicado en $($Definition.url)."
}

function Backup-SourcePathsToExternalD {
    param(
        [Parameter(Mandatory = $true)]$SourcePaths,
        $DeletePaths = @()
    )

    $changes = New-ChangeBucket
    Assert-Bucket -Value $changes -Name "Cambios de respaldo D"
    try {
        $suiteDrive = [System.IO.Path]::GetPathRoot([System.IO.Path]::GetFullPath($suiteRoot))
        if ($suiteDrive.Equals("D:\", [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-Log "[OK] Respaldo D omitido: la suite ya se esta ejecutando desde D:."
            return $changes.ToArray()
        }

        if (-not (Test-Path -LiteralPath "D:\" -PathType Container)) {
            Write-Log "[OK] Respaldo D omitido: D: no esta disponible o no esta conectado."
            return $changes.ToArray()
        }

        New-Item -ItemType Directory -Force -Path $externalDriveRoot | Out-Null
        $uniquePaths = @($SourcePaths | ForEach-Object { Normalize-RelativePath -Path ([string]$_) } | Sort-Object -Unique)
        foreach ($relative in $uniquePaths) {
            $source = Join-RootRelative -Root $suiteRoot -RelativePath $relative
            if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { continue }
            $destination = Join-RootRelative -Root $externalDriveRoot -RelativePath $relative
            $sourceKB = Get-FileKilobytesOrNull -Path $source
            $destinationKB = Get-FileKilobytesOrNull -Path $destination
            if ($null -eq $destinationKB -or $sourceKB -ne $destinationKB) {
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
                [System.IO.File]::Copy($source, $destination, $true)
                Add-Change -Changes $changes -Destination "D:\MiausoftSuite" -RelativePath $relative `
                    -PreviousKB $destinationKB -CurrentKB $sourceKB -Kind "respaldo"
            }
        }

        $uniqueDeletes = @($DeletePaths | ForEach-Object { Normalize-RelativePath -Path ([string]$_) } | Sort-Object -Unique)
        foreach ($relative in $uniqueDeletes) {
            if ([string]::IsNullOrWhiteSpace($relative)) { continue }
            $source = Join-RootRelative -Root $suiteRoot -RelativePath $relative
            if (Test-Path -LiteralPath $source -PathType Leaf) { continue }
            $destination = Join-RootRelative -Root $externalDriveRoot -RelativePath $relative
            if (Test-Path -LiteralPath $destination -PathType Leaf) {
                $destinationKB = Get-FileKilobytesOrNull -Path $destination
                Remove-Item -LiteralPath $destination -Force
                Add-Deletion -Changes $changes -Destination "D:\MiausoftSuite" -RelativePath $relative `
                    -PreviousKB $destinationKB -Kind "respaldo eliminado"
            }
        }
    } catch {
        Write-Log "[AVISO] Respaldo D omitido sin detener el publicador: $($_.Exception.Message)"
    }
    return $changes.ToArray()
}

function Write-UpdatedListFile {
    param(
        [Parameter(Mandatory = $true)]$GitChanges,
        [Parameter(Mandatory = $true)]$BackupChanges
    )

    $lines = New-Object 'System.Collections.Generic.List[string]'
    $lines.Add("MiausoftSuite - archivos actualizados o eliminados por cambio de KB") | Out-Null
    $lines.Add("Fecha: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") | Out-Null
    $lines.Add("Destino: $Target") | Out-Null
    $lines.Add("") | Out-Null

    $all = @($GitChanges) + @($BackupChanges)
    if ($all.Count -eq 0) {
        $lines.Add("Sin archivos actualizados ni eliminados por KB.") | Out-Null
    } else {
        foreach ($item in ($all | Sort-Object Destination, RelativePath -Unique)) {
            $kind = if ($null -eq $item.CurrentKB) { "eliminado" } else { $item.Kind }
            $lines.Add(("{0}`t{1}`t{2}`t{3}`t{4}" -f $item.Destination, $item.RelativePath, $kind, (Format-KBValue $item.PreviousKB), (Format-KBValue $item.CurrentKB))) | Out-Null
        }
    }

    [System.IO.File]::WriteAllLines($listPath, $lines.ToArray(), (New-Object System.Text.UTF8Encoding($false)))
    Write-Log "Lista unica de archivos actualizados: $listPath"
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
    "Criterio: solo archivos cuyo tamano en KB cambio",
    "Respaldo externo opcional: D:\MiausoftSuite",
    ""
)

$script:GitExe = Resolve-Git
if ($Login) {
    Ensure-GitHubAuthentication -InteractiveLogin
    Write-Host ""
    Write-Host "Inicio de sesion GitHub para Git completado." -ForegroundColor Green
    Write-Host "Registro: $logPath"
    exit 0
}

$mutex = New-Object System.Threading.Mutex($false, "MiausoftSuite.Publisher")
$ownsMutex = $false
$allSourcePaths = New-PathBucket
$allGitChanges = New-ChangeBucket
Assert-Bucket -Value $allSourcePaths -Name "Rutas globales de publicacion"
Assert-Bucket -Value $allGitChanges -Name "Cambios globales de publicacion"
$backupChanges = @()
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
        $prepared = Prepare-TlalpowaSnapshot -Repository $repository
        Add-PathList -Destination $allSourcePaths -Paths $prepared.SourcePaths
        Add-ChangeList -Destination $allGitChanges -Items $prepared.Changes
        Write-ChangeSummary -Title "GitHub/Tlalpowa" -Changes $prepared.Changes
        Publish-Repository -Repository $repository -Name "Tlalpowa" -Definition $definition `
            -ChangedFiles $prepared.Changes `
            -Message ("Publicar Tlalpowa {0}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    }

    if ($Target -ne "Tlalpowa") {
        $definition = $configuration.repositories.MiausoftSuite
        $repository = Ensure-Clone -Name "MiausoftSuite" -Definition $definition
        if ($Target -in @("Suite", "Todo")) {
            $prepared = Prepare-FullSuiteSnapshot -Repository $repository
        } else {
            $prepared = Prepare-SelectedSuiteSnapshot -Repository $repository -Selection $Target
        }
        Add-PathList -Destination $allSourcePaths -Paths $prepared.SourcePaths
        Add-ChangeList -Destination $allGitChanges -Items $prepared.Changes
        Write-ChangeSummary -Title "GitHub/MiausoftSuite" -Changes $prepared.Changes
        Publish-Repository -Repository $repository -Name "MiausoftSuite" -Definition $definition `
            -ChangedFiles $prepared.Changes `
            -Message ("Publicar {0} {1}" -f $Target, (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    }

    if ($DryRun) {
        Write-Log "[SIMULACION] Respaldo D:\MiausoftSuite omitido."
        $backupChanges = @()
    } else {
        $deletePaths = @($allGitChanges.ToArray() | Where-Object { $null -eq $_.CurrentKB } | ForEach-Object { $_.RelativePath })
        $backupChanges = Backup-SourcePathsToExternalD -SourcePaths $allSourcePaths.ToArray() -DeletePaths $deletePaths
    }
    Write-ChangeSummary -Title "Respaldo D:\MiausoftSuite" -Changes $backupChanges
    Write-UpdatedListFile -GitChanges $allGitChanges.ToArray() -BackupChanges $backupChanges

    Write-Host ""
    Write-Host $(if ($DryRun) { "Simulacion completada." } else { "Publicacion completada." }) -ForegroundColor Green
    Write-Host "Registro: $logPath"
    Write-Host "Lista: $listPath"
} finally {
    if ($ownsMutex) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
}
