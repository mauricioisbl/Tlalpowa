[CmdletBinding()]
param(
    [ValidateSet("", "Tlalpowa", "Ilnamiki", "Organizador", "Suite", "Todo")]
    [string]$Target = "",
    [switch]$DryRun,
    [switch]$Compile,
    [switch]$NoCompile,
    [switch]$BackupD,
    [switch]$Login
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2.0

$script:Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$script:Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$script:StateRoot = Join-Path $env:LOCALAPPDATA "MiausoftSuite\Tecnico\Publicador"
$script:RepoRoot = Join-Path $script:StateRoot "Repos"
$script:ListRoot = Join-Path $script:StateRoot "Listas"
$script:IndexRoot = Join-Path $script:StateRoot "Indices"
$script:LogPath = Join-Path $script:Root "PUBLICAR.LOG"
$script:ConfigPath = Join-Path $PSScriptRoot "Repositorios.json"
$script:Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$script:GitExe = $null
$script:Config = $null

New-Item -ItemType Directory -Force -Path $script:StateRoot, $script:RepoRoot, $script:ListRoot, $script:IndexRoot | Out-Null
[System.IO.File]::WriteAllText($script:LogPath, "", $script:Utf8NoBom)

function Write-Log {
    param([string]$Text = "")
    $Text | Out-Host
    [System.IO.File]::AppendAllText($script:LogPath, $Text + [Environment]::NewLine, $script:Utf8NoBom)
}

function Write-Section {
    param([string]$Text)
    Write-Log ""
    Write-Log "============================================================"
    Write-Log (" {0}" -f $Text)
    Write-Log "============================================================"
}

function Resolve-Git {
    $command = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($command) {
        if ($command.Source) { return $command.Source }
        return $command.Path
    }
    $fallback = "C:\Program Files\Git\cmd\git.exe"
    if (Test-Path -LiteralPath $fallback -PathType Leaf) { return $fallback }
    throw "No se encontro git.exe. Instala Git para Windows antes de publicar."
}

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string[]]$Arguments = @(),
        [string]$WorkingDirectory = $script:Root,
        [switch]$AllowFailure,
        [switch]$Quiet
    )

    if (-not $Quiet) {
        $printable = (($Arguments | ForEach-Object {
            if ($_ -match "\s") { '"{0}"' -f $_ } else { $_ }
        }) -join " ")
        Write-Log (("> {0} {1}" -f ([System.IO.Path]::GetFileName($FilePath)), $printable).Trim())
    }

    $oldErrorAction = $ErrorActionPreference
    Push-Location $WorkingDirectory
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& $FilePath @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorAction
        Pop-Location
    }

    if (-not $Quiet) {
        foreach ($line in $output) {
            if (-not [string]::IsNullOrWhiteSpace([string]$line)) {
                Write-Log ("  {0}" -f [string]$line)
            }
        }
    }
    if ($exitCode -ne 0 -and -not $AllowFailure) {
        throw ("{0} termino con codigo {1}." -f ([System.IO.Path]::GetFileName($FilePath)), $exitCode)
    }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = $output
    }
}

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [string]$WorkingDirectory = $script:Root,
        [switch]$AllowFailure,
        [switch]$Quiet
    )
    return Invoke-External -FilePath $script:GitExe -Arguments $Arguments -WorkingDirectory $WorkingDirectory -AllowFailure:$AllowFailure -Quiet:$Quiet
}

function Read-RepositoryConfig {
    if (-not (Test-Path -LiteralPath $script:ConfigPath -PathType Leaf)) {
        throw "No se encontro la configuracion de repositorios: $script:ConfigPath"
    }
    try {
        $config = Get-Content -LiteralPath $script:ConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
    } catch {
        throw ("Repositorios.json no es JSON valido: {0}" -f $_.Exception.Message)
    }
    if (-not $config.repositories) { throw "Repositorios.json no contiene 'repositories'." }
    return $config
}

function New-RepoSpec {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][object[]]$Mappings,
        [switch]$ReplaceRepository
    )

    $entry = $script:Config.repositories.$Name
    if (-not $entry -or [string]::IsNullOrWhiteSpace([string]$entry.url)) {
        throw "Falta repositories.$Name.url en Repositorios.json."
    }
    $branch = [string]$entry.branch
    if ([string]::IsNullOrWhiteSpace($branch)) { $branch = "main" }
    if ($branch -notmatch "^[A-Za-z0-9._/-]+$") { throw "Rama no valida para ${Name}: $branch" }

    return [pscustomobject]@{
        Name = $Name
        Url = [string]$entry.url
        Branch = $branch
        Mappings = @($Mappings)
        ReplaceRepository = [bool]$ReplaceRepository
        WorkDir = Join-Path $script:RepoRoot $Name
    }
}

function New-PathMapping {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [string]$Destination = ""
    )
    if ([string]::IsNullOrWhiteSpace($Destination)) { $Destination = $Source }
    return [pscustomobject]@{
        Source = Normalize-RelativePath $Source
        Destination = Normalize-RelativePath $Destination
    }
}

function Get-SpecsForTarget {
    param([Parameter(Mandatory = $true)][string]$RequestedTarget)

    $shared = @(
        (New-PathMapping "compilepushpull.cmd"),
        (New-PathMapping "directorio.py"),
        (New-PathMapping "core")
    )
    $tlalpowa = New-RepoSpec -Name "Tlalpowa" -Mappings @(
        (New-PathMapping "compilepushpull.cmd"),
        (New-PathMapping "directorio.py"),
        (New-PathMapping "Tlalpowa.exe"),
        (New-PathMapping "core" "CORE"),
        (New-PathMapping "tlalpowa" "TLALPOWA"),
        (New-PathMapping "datos" "TLALPOWA/Datos")
    )
    $ilnamikiMappings = $shared + @(
        (New-PathMapping "Ilnamiki.exe"),
        (New-PathMapping "ilnamiki")
    )
    $organizadorMappings = $shared + @(
        (New-PathMapping "Organizador.exe"),
        (New-PathMapping "miausoftools/Organizador_Biblioteca")
    )
    $suiteMappings = $shared + @(
        (New-PathMapping "Ilnamiki.exe"),
        (New-PathMapping "ilnamiki"),
        (New-PathMapping "Organizador.exe"),
        (New-PathMapping "miausoftools/Organizador_Biblioteca")
    )

    switch ($RequestedTarget) {
        "Tlalpowa" { return @($tlalpowa) }
        "Ilnamiki" { return @(New-RepoSpec -Name "MiausoftSuite" -Mappings $ilnamikiMappings) }
        "Organizador" { return @(New-RepoSpec -Name "MiausoftSuite" -Mappings $organizadorMappings) }
        "Suite" { return @(New-RepoSpec -Name "MiausoftSuite" -Mappings $suiteMappings -ReplaceRepository) }
        "Todo" {
            return @(
                $tlalpowa,
                (New-RepoSpec -Name "MiausoftSuite" -Mappings $suiteMappings -ReplaceRepository)
            )
        }
        default { throw "Destino no indicado." }
    }
}

function Normalize-RelativePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return ($Path -replace "\\", "/").TrimStart("/")
}

function Convert-RelativeToPath {
    param(
        [Parameter(Mandatory = $true)][string]$Base,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )
    $relative = (Normalize-RelativePath $RelativePath) -replace "/", [System.IO.Path]::DirectorySeparatorChar
    return [System.IO.Path]::GetFullPath((Join-Path $Base $relative))
}

function Get-RelativeFromRoot {
    param([Parameter(Mandatory = $true)][string]$Path)
    $rootPrefix = $script:Root.TrimEnd("\", "/") + [System.IO.Path]::DirectorySeparatorChar
    $full = [System.IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) { return $null }
    return Normalize-RelativePath $full.Substring($rootPrefix.Length)
}

function Test-ExcludedRelativePath {
    param([Parameter(Mandatory = $true)][string]$RelativePath)
    $path = Normalize-RelativePath $RelativePath
    $leaf = [System.IO.Path]::GetFileName($path)

    if ($path -match "(^|/)\.git(/|$)") { return $true }
    if ($path -match "(^|/)(descargas|\.vs|__pycache__|node_modules|\.cache|\.pytest_cache|_estado|Tecnico)(/|$)") { return $true }
    if ($path -match "(^|/)(build|_build|out|CMakeFiles)(/|$)") { return $true }
    if ($path -match "^core/BuildSystem/VisualContract(/|$)") { return $true }
    if ($path -match "(^|/)cmake-build-[^/]+(/|$)") { return $true }
    if ($path -match "(^|/)\.ninja_") { return $true }
    if ($path -match "\.(ilk|pdb|obj|pch|idb|ipch|tlog|lastbuildstate|tmp|temp|log|user)$") { return $true }
    if ($path.Equals("PUBLICAR.LOG", [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
    if ($leaf.Equals("Pasted text.txt", [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
    if ($leaf.Equals("rutas_directorio.txt", [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
    if ($path.Equals("core/Dependencias/dependencies.local.json", [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
    if ($leaf.StartsWith("MiausoftSuite_Publicador", [System.StringComparison]::OrdinalIgnoreCase) -and $leaf.EndsWith(".zip", [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
    if ($leaf.StartsWith("MiausoftSuite_Tlalpowa_", [System.StringComparison]::OrdinalIgnoreCase) -and $leaf.EndsWith(".zip", [System.StringComparison]::OrdinalIgnoreCase)) { return $true }

    $legacyIlnamiki = @(
        "ilnamiki/index.html",
        "ilnamiki/ilnamiki.css",
        "ilnamiki/ilnamiki.js",
        "ilnamiki/ilnamiki.sql"
    )
    foreach ($legacy in $legacyIlnamiki) {
        if ($path.Equals($legacy, [System.StringComparison]::OrdinalIgnoreCase)) { return $true }
    }
    return $false
}

function Get-SourceFiles {
    param([Parameter(Mandatory = $true)]$Spec)
    $files = @{}
    foreach ($mapping in $Spec.Mappings) {
        $source = Convert-RelativeToPath -Base $script:Root -RelativePath $mapping.Source
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            $localRelative = Get-RelativeFromRoot $source
            if ($localRelative -and -not (Test-ExcludedRelativePath $localRelative)) {
                $files[$mapping.Destination] = $source
            }
        } elseif (Test-Path -LiteralPath $source -PathType Container) {
            foreach ($item in Get-ChildItem -LiteralPath $source -Recurse -File -Force -ErrorAction SilentlyContinue) {
                $localRelative = Get-RelativeFromRoot $item.FullName
                if (-not $localRelative -or (Test-ExcludedRelativePath $localRelative)) { continue }
                $sourcePrefix = $mapping.Source.TrimEnd("/") + "/"
                $suffix = $localRelative.Substring($sourcePrefix.Length)
                $remoteRelative = $mapping.Destination.TrimEnd("/") + "/" + $suffix
                $files[$remoteRelative] = $item.FullName
            }
        } else {
            throw ("Falta una ruta requerida para {0}: {1}" -f $Spec.Name, $mapping.Source)
        }
    }
    return $files
}

function Assert-PublishableFileSizes {
    param(
        [Parameter(Mandatory = $true)]$Spec,
        [Parameter(Mandatory = $true)]$SourceFiles
    )
    $limit = 100MB
    $tooLarge = @()
    foreach ($relative in $SourceFiles.Keys) {
        $item = Get-Item -LiteralPath $SourceFiles[$relative] -Force
        if ($item.Length -gt $limit) {
            $tooLarge += ("{0} ({1:N1} MiB)" -f $relative, ($item.Length / 1MB))
        }
    }
    if ($tooLarge.Count -gt 0) {
        throw ("{0} contiene archivos mayores de 100 MiB, limite de un objeto GitHub normal. Configura Git LFS o excluyelos:`n{1}" -f $Spec.Name, ($tooLarge -join "`n"))
    }
}

function Assert-SafeTechnicalPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    $statePrefix = [System.IO.Path]::GetFullPath($script:StateRoot).TrimEnd("\", "/") + [System.IO.Path]::DirectorySeparatorChar
    $resolved = [System.IO.Path]::GetFullPath($Path)
    if (-not $resolved.StartsWith($statePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Ruta tecnica fuera del area segura: $resolved"
    }
}

function Remove-TechnicalRepository {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    Assert-SafeTechnicalPath $Path
    Get-ChildItem -LiteralPath $Path -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object {
        try { $_.Attributes = [System.IO.FileAttributes]::Normal } catch { }
    }
    Remove-Item -LiteralPath $Path -Recurse -Force
}

function Test-TechnicalRepository {
    param([Parameter(Mandatory = $true)]$Spec)
    $gitDirectory = Join-Path $Spec.WorkDir ".git"
    if (-not (Test-Path -LiteralPath $gitDirectory -PathType Container)) { return $false }
    $staleLocks = @(Get-ChildItem -LiteralPath $gitDirectory -Filter "*.lock" -Recurse -Force -File -ErrorAction SilentlyContinue)
    if ($staleLocks.Count -gt 0) {
        Write-Log ("[AVISO] Cache tecnica con {0} bloqueo(s) residual(es); se reconstruira." -f $staleLocks.Count)
        return $false
    }
    $result = Invoke-Git -Arguments @("rev-parse", "--git-dir") -WorkingDirectory $Spec.WorkDir -AllowFailure -Quiet
    return ($result.ExitCode -eq 0)
}

function New-TechnicalRepository {
    param([Parameter(Mandatory = $true)]$Spec)
    Remove-TechnicalRepository $Spec.WorkDir
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Spec.WorkDir) | Out-Null
    Write-Log ("[INFO] Creando indice tecnico parcial para {0}; no se descargan los archivos pesados." -f $Spec.Name)
    $arguments = @(
        "-c", "core.longpaths=true",
        "clone",
        "--filter=blob:none",
        "--no-checkout",
        "--no-tags",
        "--depth", "1",
        "--single-branch",
        "--branch", $Spec.Branch,
        $Spec.Url,
        $Spec.WorkDir
    )
    $result = Invoke-Git -Arguments $arguments -WorkingDirectory $script:RepoRoot -AllowFailure
    if ($result.ExitCode -ne 0 -or -not (Test-TechnicalRepository $Spec)) {
        Remove-TechnicalRepository $Spec.WorkDir
        throw ("No pude crear el repositorio tecnico de {0}. Revisa internet, URL y acceso a GitHub." -f $Spec.Name)
    }
}

function Reset-TechnicalRepositoryToRemote {
    param([Parameter(Mandatory = $true)]$Spec)
    if (-not (Test-TechnicalRepository $Spec)) { New-TechnicalRepository $Spec }

    Invoke-Git -Arguments @("config", "core.longpaths", "true") -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
    Invoke-Git -Arguments @("config", "core.autocrlf", "false") -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
    Invoke-Git -Arguments @("config", "core.filemode", "false") -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
    Invoke-Git -Arguments @("config", "gc.auto", "0") -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
    Invoke-Git -Arguments @("config", "maintenance.auto", "false") -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
    Invoke-Git -Arguments @("config", "user.name", "Miausoft Publisher") -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
    Invoke-Git -Arguments @("config", "user.email", "miausoft-publisher@users.noreply.github.com") -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null

    $remote = Invoke-Git -Arguments @("remote", "get-url", "origin") -WorkingDirectory $Spec.WorkDir -AllowFailure -Quiet
    if ($remote.ExitCode -ne 0) {
        Invoke-Git -Arguments @("remote", "add", "origin", $Spec.Url) -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
    } else {
        $currentUrl = ((@($remote.Output) | ForEach-Object { [string]$_ }) -join "").Trim()
        if (-not $currentUrl.Equals($Spec.Url, [System.StringComparison]::OrdinalIgnoreCase)) {
            Invoke-Git -Arguments @("remote", "set-url", "origin", $Spec.Url) -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
        }
    }

    Write-Log ("[INFO] Actualizando {0}/origin/{1}..." -f $Spec.Name, $Spec.Branch)
    $refspec = "+refs/heads/{0}:refs/remotes/origin/{0}" -f $Spec.Branch
    $fetch = Invoke-Git -Arguments @(
        "-c", "fetch.writeCommitGraph=false",
        "fetch", "--filter=blob:none", "--no-tags", "--prune", "--depth", "1", "origin", $refspec
    ) -WorkingDirectory $Spec.WorkDir -AllowFailure
    if ($fetch.ExitCode -ne 0) {
        Write-Log "[AVISO] La cache tecnica no pudo actualizarse; se reconstruira una vez."
        New-TechnicalRepository $Spec
        $fetch = Invoke-Git -Arguments @(
            "-c", "fetch.writeCommitGraph=false",
            "fetch", "--filter=blob:none", "--no-tags", "--prune", "--depth", "1", "origin", $refspec
        ) -WorkingDirectory $Spec.WorkDir -AllowFailure
        if ($fetch.ExitCode -ne 0) { throw ("No pude leer origin/{0} de {1}." -f $Spec.Branch, $Spec.Name) }
    }

    $remoteRef = "refs/remotes/origin/{0}" -f $Spec.Branch
    $branchRef = "refs/heads/{0}" -f $Spec.Branch
    Invoke-Git -Arguments @("symbolic-ref", "HEAD", $branchRef) -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
    Invoke-Git -Arguments @("update-ref", $branchRef, $remoteRef) -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
}

function Invoke-GitWithStandardInput {
    param(
        [Parameter(Mandatory = $true)]$Spec,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$InputText
    )

    $argumentText = (($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') { '"{0}"' -f ($_ -replace '"', '\"') } else { $_ }
    }) -join " ")
    $normalizedInput = $InputText.TrimStart([char]0xFEFF)
    $bytes = $script:Utf8NoBom.GetBytes($normalizedInput)
    $inputPath = Join-Path $script:IndexRoot ("index-info-{0}-{1}-{2}.txt" -f $Spec.Name, $script:Stamp, [Guid]::NewGuid().ToString("N"))
    [System.IO.File]::WriteAllBytes($inputPath, $bytes)
    try {
        $lastDetail = ""
        for ($attempt = 1; $attempt -le 3; $attempt++) {
            Write-Log ("> git.exe {0} <indice:{1} bytes; intento {2}/3>" -f $argumentText, $bytes.Length, $attempt)
            $commandText = '""{0}" {1} < "{2}""' -f $script:GitExe, $argumentText, $inputPath
            $startInfo = New-Object System.Diagnostics.ProcessStartInfo
            $startInfo.FileName = "cmd.exe"
            $startInfo.Arguments = "/d /s /c $commandText"
            $startInfo.WorkingDirectory = $Spec.WorkDir
            $startInfo.UseShellExecute = $false
            $startInfo.CreateNoWindow = $true
            $startInfo.RedirectStandardOutput = $true
            $startInfo.RedirectStandardError = $true

            $process = New-Object System.Diagnostics.Process
            $process.StartInfo = $startInfo
            try {
                if (-not $process.Start()) { throw "No se pudo iniciar git.exe." }
                $stdout = $process.StandardOutput.ReadToEnd()
                $stderr = $process.StandardError.ReadToEnd()
                $process.WaitForExit()
                if (-not [string]::IsNullOrWhiteSpace($stdout)) { Write-Log ("  {0}" -f $stdout.Trim()) }
                if (-not [string]::IsNullOrWhiteSpace($stderr)) { Write-Log ("  {0}" -f $stderr.Trim()) }
                if ($process.ExitCode -eq 0) { return }

                $lastDetail = $stderr.Trim()
                if ([string]::IsNullOrWhiteSpace($lastDetail)) { $lastDetail = $stdout.Trim() }
                Write-Log ("[AVISO] Git rechazo el indice: {0}" -f $lastDetail)
            } finally {
                $process.Dispose()
            }
            if ($attempt -lt 3) { Start-Sleep -Milliseconds (300 * $attempt) }
        }
        throw ("git.exe no pudo recibir el indice despues de tres intentos: {0}" -f $lastDetail)
    } finally {
        Remove-Item -LiteralPath $inputPath -Force -ErrorAction SilentlyContinue
    }
}

function Get-RemoteTreeInfo {
    param([Parameter(Mandatory = $true)]$Spec)
    $reference = "refs/remotes/origin/{0}" -f $Spec.Branch
    $result = Invoke-Git -Arguments @("-c", "core.quotePath=false", "ls-tree", "-r", $reference) -WorkingDirectory $Spec.WorkDir -Quiet
    $byPath = @{}
    $hashes = @{}
    foreach ($lineObject in @($result.Output)) {
        $line = [string]$lineObject
        if ($line -match "^([0-9]{6})\s+blob\s+([0-9a-fA-F]{40,64})`t(.+)$") {
            $path = Normalize-RelativePath $Matches[3]
            $byPath[$path] = [pscustomobject]@{ Mode = $Matches[1]; Hash = $Matches[2].ToLowerInvariant() }
            $hashes[$Matches[2].ToLowerInvariant()] = $true
        }
    }
    return [pscustomobject]@{ ByPath = $byPath; Hashes = $hashes }
}

function Get-MinimalDestinationRoots {
    param([Parameter(Mandatory = $true)]$Spec)
    $destinations = @($Spec.Mappings | ForEach-Object { $_.Destination.TrimEnd("/") } | Sort-Object Length, @{ Expression = { $_ } })
    $roots = New-Object System.Collections.Generic.List[string]
    foreach ($destination in $destinations) {
        $covered = $false
        foreach ($root in $roots) {
            if ($destination.Equals($root, [System.StringComparison]::OrdinalIgnoreCase) -or
                $destination.StartsWith($root + "/", [System.StringComparison]::OrdinalIgnoreCase)) {
                $covered = $true
                break
            }
        }
        if (-not $covered) { $roots.Add($destination) | Out-Null }
    }
    return $roots.ToArray()
}

function Get-SourceObjectInfo {
    param(
        [Parameter(Mandatory = $true)]$Spec,
        [Parameter(Mandatory = $true)]$SourceFiles,
        [Parameter(Mandatory = $true)]$RemoteTree
    )
    $items = New-Object System.Collections.Generic.List[object]
    $paths = @($SourceFiles.Keys | Sort-Object)
    for ($offset = 0; $offset -lt $paths.Count; $offset += 30) {
        $last = [Math]::Min($paths.Count - 1, $offset + 29)
        $chunk = @($paths[$offset..$last])
        $arguments = @("hash-object", "--") + @($chunk | ForEach-Object { $SourceFiles[$_] })
        $result = Invoke-Git -Arguments $arguments -WorkingDirectory $Spec.WorkDir -Quiet
        if ($result.Output.Count -ne $chunk.Count) {
            throw ("Git devolvio {0} hashes para {1} archivos." -f $result.Output.Count, $chunk.Count)
        }
        for ($index = 0; $index -lt $chunk.Count; $index++) {
            $remotePath = $chunk[$index]
            $hash = ([string]$result.Output[$index]).Trim().ToLowerInvariant()
            $mode = "100644"
            if ($RemoteTree.ByPath.ContainsKey($remotePath)) { $mode = $RemoteTree.ByPath[$remotePath].Mode }
            $items.Add([pscustomobject]@{
                RemotePath = $remotePath
                SourcePath = $SourceFiles[$remotePath]
                Hash = $hash
                Mode = $mode
            }) | Out-Null
        }
    }
    return $items.ToArray()
}

function Stage-Source {
    param(
        [Parameter(Mandatory = $true)]$Spec,
        [Parameter(Mandatory = $true)]$SourceFiles
    )
    $remoteTree = Get-RemoteTreeInfo $Spec
    $objects = @(Get-SourceObjectInfo -Spec $Spec -SourceFiles $SourceFiles -RemoteTree $remoteTree)

    if ($Spec.ReplaceRepository) {
        Write-Log ("[INFO] {0}: reemplazo controlado del repositorio con los componentes vigentes." -f $Spec.Name)
        Invoke-Git -Arguments @("rm", "-r", "-f", "--cached", "--ignore-unmatch", "--", ".") -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
    } else {
        foreach ($destination in Get-MinimalDestinationRoots $Spec) {
            Invoke-Git -Arguments @("rm", "-r", "-f", "--cached", "--ignore-unmatch", "--", $destination) -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
        }
    }

    $writtenHashes = @{}
    $objectsToWrite = New-Object System.Collections.Generic.List[object]
    foreach ($item in $objects) {
        if ($remoteTree.Hashes.ContainsKey($item.Hash) -or $writtenHashes.ContainsKey($item.Hash)) { continue }
        $writtenHashes[$item.Hash] = $true
        $objectsToWrite.Add($item) | Out-Null
    }
    for ($offset = 0; $offset -lt $objectsToWrite.Count; $offset += 30) {
        $last = [Math]::Min($objectsToWrite.Count - 1, $offset + 29)
        $chunk = @($objectsToWrite[$offset..$last])
        $write = Invoke-Git -Arguments (@("hash-object", "-w", "--") + @($chunk | ForEach-Object { $_.SourcePath })) -WorkingDirectory $Spec.WorkDir -Quiet
        if ($write.Output.Count -ne $chunk.Count) {
            throw ("Git escribio {0} objetos para {1} archivos." -f $write.Output.Count, $chunk.Count)
        }
        for ($index = 0; $index -lt $chunk.Count; $index++) {
            $writtenHash = ([string]$write.Output[$index]).Trim().ToLowerInvariant()
            if (-not $writtenHash.Equals($chunk[$index].Hash, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw ("Hash inestable al preparar {0}." -f $chunk[$index].RemotePath)
            }
        }
    }

    $indexLines = New-Object System.Collections.Generic.List[string]
    foreach ($item in $objects) {
        $indexLines.Add(("{0} {1}`t{2}" -f $item.Mode, $item.Hash, $item.RemotePath)) | Out-Null
    }
    Invoke-GitWithStandardInput -Spec $Spec -Arguments @("update-index", "--add", "--index-info") -InputText (($indexLines -join "`n") + "`n")
    Write-Log ("[OK] Indice exacto construido: {0} archivo(s), {1} objeto(s) nuevo(s)." -f $objects.Count, $objectsToWrite.Count)
}

function Get-StagedChanges {
    param([Parameter(Mandatory = $true)]$Spec)
    $result = Invoke-Git -Arguments @("-c", "core.quotePath=false", "diff", "--cached", "--name-status", "--no-renames") -WorkingDirectory $Spec.WorkDir -Quiet
    $changes = New-Object System.Collections.Generic.List[object]
    foreach ($lineObject in @($result.Output)) {
        $line = [string]$lineObject
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        $parts = $line.Split("`t")
        if ($parts.Count -lt 2) { continue }
        $status = $parts[0]
        if (($status.StartsWith("R") -or $status.StartsWith("C")) -and $parts.Count -ge 3) {
            $path = ("{0} -> {1}" -f $parts[1], $parts[2])
        } else {
            $path = $parts[1]
        }
        $changes.Add([pscustomobject]@{ Status = $status; Path = $path }) | Out-Null
    }
    return $changes.ToArray()
}

function Write-ChangeList {
    param(
        [Parameter(Mandatory = $true)]$Spec,
        [Parameter(Mandatory = $true)]$Changes
    )
    $file = Join-Path $script:ListRoot ("GitHub-{0}-{1}.txt" -f $Spec.Name, $script:Stamp)
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add(("Repositorio: {0}" -f $Spec.Name))
    $lines.Add(("Destino: {0}" -f $Spec.Url))
    $lines.Add(("Generado: {0}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss")))
    $lines.Add(("Total: {0}" -f $Changes.Count))
    $lines.Add("")
    foreach ($change in $Changes) { $lines.Add(("{0}`t{1}" -f $change.Status, $change.Path)) }
    [System.IO.File]::WriteAllLines($file, $lines.ToArray(), $script:Utf8NoBom)
    Write-Log ("[OK] Lista completa: {0}" -f $file)
}

function Show-Changes {
    param(
        [Parameter(Mandatory = $true)]$Spec,
        [Parameter(Mandatory = $true)]$Changes
    )
    Write-Log ("[LISTA] GitHub/{0}: {1} cambio(s) exacto(s)." -f $Spec.Name, $Changes.Count)
    foreach ($change in @($Changes | Select-Object -First 50)) {
        Write-Log ("  {0}`t{1}" -f $change.Status, $change.Path)
    }
    if ($Changes.Count -gt 50) {
        Write-Log ("  ... {0} cambio(s) mas; consulta la lista completa." -f ($Changes.Count - 50))
    }
}

function Test-StagedChanges {
    param([Parameter(Mandatory = $true)]$Spec)
    $result = Invoke-Git -Arguments @("diff", "--cached", "--quiet", "--exit-code") -WorkingDirectory $Spec.WorkDir -AllowFailure -Quiet
    return ($result.ExitCode -eq 1)
}

function Get-CommitId {
    param(
        [Parameter(Mandatory = $true)]$Spec,
        [Parameter(Mandatory = $true)][string]$Reference
    )
    $result = Invoke-Git -Arguments @("rev-parse", "--verify", $Reference) -WorkingDirectory $Spec.WorkDir -Quiet
    return ((@($result.Output) | ForEach-Object { [string]$_ }) -join "").Trim()
}

function Verify-RemoteCommit {
    param(
        [Parameter(Mandatory = $true)]$Spec,
        [Parameter(Mandatory = $true)][string]$ExpectedCommit
    )
    $refspec = "+refs/heads/{0}:refs/remotes/origin/{0}" -f $Spec.Branch
    Invoke-Git -Arguments @("fetch", "--filter=blob:none", "--no-tags", "--depth", "1", "origin", $refspec) -WorkingDirectory $Spec.WorkDir | Out-Null
    $remoteCommit = Get-CommitId -Spec $Spec -Reference ("refs/remotes/origin/{0}" -f $Spec.Branch)
    if (-not $remoteCommit.Equals($ExpectedCommit, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw ("Verificacion fallida: GitHub tiene {0}, pero se esperaba {1}." -f $remoteCommit, $ExpectedCommit)
    }
    Write-Log ("[OK] Verificado en GitHub: origin/{0} = {1}" -f $Spec.Branch, $ExpectedCommit)
}

function Test-FilesEqual {
    param(
        [Parameter(Mandatory = $true)][string]$First,
        [Parameter(Mandatory = $true)][string]$Second
    )
    if (-not (Test-Path -LiteralPath $Second -PathType Leaf)) { return $false }
    $a = Get-Item -LiteralPath $First -Force
    $b = Get-Item -LiteralPath $Second -Force
    if ($a.Length -ne $b.Length) { return $false }
    if ($a.Length -eq 0) { return $true }
    return (Get-FileHash -LiteralPath $First -Algorithm SHA256).Hash.Equals(
        (Get-FileHash -LiteralPath $Second -Algorithm SHA256).Hash,
        [System.StringComparison]::OrdinalIgnoreCase
    )
}

function Sync-DBackup {
    param(
        [Parameter(Mandatory = $true)]$Spec,
        [Parameter(Mandatory = $true)]$SourceFiles
    )
    if ($DryRun) {
        Write-Log "[SIMULACION] Respaldo D omitido."
        return
    }
    $sourceDrive = [System.IO.Path]::GetPathRoot($script:Root)
    if ($sourceDrive -and $sourceDrive.Equals("D:\", [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Log "[OK] Respaldo D omitido: el origen ya esta en D:."
        return
    }
    if (-not (Test-Path -LiteralPath "D:\" -PathType Container)) {
        Write-Log "[OK] Respaldo D omitido: D: no esta disponible."
        return
    }

    $backupRoot = "D:\MiausoftSuite"
    New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
    $copied = 0
    foreach ($remoteRelative in ($SourceFiles.Keys | Sort-Object)) {
        $source = $SourceFiles[$remoteRelative]
        $localRelative = Get-RelativeFromRoot $source
        if (-not $localRelative) { continue }
        $destination = Convert-RelativeToPath -Base $backupRoot -RelativePath $localRelative
        if (-not (Test-FilesEqual -First $source -Second $destination)) {
            $parent = Split-Path -Parent $destination
            if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
                New-Item -ItemType Directory -Force -Path $parent | Out-Null
            }
            Copy-Item -LiteralPath $source -Destination $destination -Force
            $copied++
        }
    }
    Write-Log ("[OK] Respaldo D verificado byte a byte: {0} archivo(s) actualizado(s)." -f $copied)
}

function Invoke-DBackupSafe {
    param(
        [Parameter(Mandatory = $true)]$Spec,
        [Parameter(Mandatory = $true)]$SourceFiles
    )
    if (-not $BackupD) {
        Write-Log "[OK] Respaldo D omitido; usa -BackupD cuando quieras sincronizarlo."
        return
    }
    try {
        Sync-DBackup -Spec $Spec -SourceFiles $SourceFiles
    } catch {
        Write-Log ("[AVISO] GitHub quedo correcto, pero el respaldo D no pudo completarse: {0}" -f $_.Exception.Message)
    }
}

function Publish-Spec {
    param([Parameter(Mandatory = $true)]$Spec)
    Write-Section ("Repositorio {0}" -f $Spec.Name)
    $sourceFiles = Get-SourceFiles $Spec
    Write-Log ("[INFO] {0}: {1} archivo(s) fuente en alcance." -f $Spec.Name, $sourceFiles.Count)
    Assert-PublishableFileSizes -Spec $Spec -SourceFiles $sourceFiles

    $lastPushOutput = ""
    for ($attempt = 1; $attempt -le 2; $attempt++) {
        Reset-TechnicalRepositoryToRemote $Spec
        $indexFile = Join-Path $script:IndexRoot ("{0}-{1}-{2}.index" -f $Spec.Name, $script:Stamp, $attempt)
        $indexLock = $indexFile + ".lock"
        Remove-Item -LiteralPath $indexFile, $indexLock -Force -ErrorAction SilentlyContinue
        $previousIndex = $env:GIT_INDEX_FILE
        try {
            $env:GIT_INDEX_FILE = $indexFile
            $remoteRef = "refs/remotes/origin/{0}" -f $Spec.Branch
            Invoke-Git -Arguments @("read-tree", "--reset", $remoteRef) -WorkingDirectory $Spec.WorkDir -Quiet | Out-Null
            Write-Log ("[INFO] Indice transaccional aislado: {0}" -f $indexFile)

            Stage-Source -Spec $Spec -SourceFiles $sourceFiles
            $changes = @(Get-StagedChanges $Spec)
            Show-Changes -Spec $Spec -Changes $changes
            Write-ChangeList -Spec $Spec -Changes $changes

            if (-not (Test-StagedChanges $Spec)) {
                Write-Log ("[OK] {0}: GitHub ya contiene exactamente los archivos del alcance." -f $Spec.Name)
                if (-not $DryRun) { Invoke-DBackupSafe -Spec $Spec -SourceFiles $sourceFiles }
                return
            }

            $message = "Publicacion MiausoftSuite $Target $($script:Stamp)"
            Invoke-Git -Arguments @("-c", "gc.auto=0", "-c", "maintenance.auto=false", "commit", "--no-verify", "-m", $message) -WorkingDirectory $Spec.WorkDir | Out-Null
            $candidate = Get-CommitId -Spec $Spec -Reference "HEAD"

            if ($DryRun) {
                $dryPush = Invoke-Git -Arguments @("push", "--dry-run", "--porcelain", "origin", ("HEAD:{0}" -f $Spec.Branch)) -WorkingDirectory $Spec.WorkDir -AllowFailure
                if ($dryPush.ExitCode -ne 0) {
                    throw ("La simulacion de push fue rechazada para {0}. Revisa credenciales, permisos o proteccion de rama." -f $Spec.Name)
                }
                Write-Log ("[OK] Simulacion completa: GitHub aceptaria el commit {0}; no se modifico el remoto." -f $candidate)
                return
            }

            $push = Invoke-Git -Arguments @("push", "--porcelain", "origin", ("HEAD:{0}" -f $Spec.Branch)) -WorkingDirectory $Spec.WorkDir -AllowFailure
            $lastPushOutput = ((@($push.Output) | ForEach-Object { [string]$_ }) -join [Environment]::NewLine)
            if ($push.ExitCode -eq 0) {
                Verify-RemoteCommit -Spec $Spec -ExpectedCommit $candidate
                Invoke-DBackupSafe -Spec $Spec -SourceFiles $sourceFiles
                Write-Log ("[OK] {0} publicado correctamente en {1}." -f $Spec.Name, $Spec.Url)
                return
            }

            if ($attempt -lt 2) {
                Write-Log "[AVISO] El push fallo; refresco origin y reconstruyo el commit una vez para resolver carreras non-fast-forward."
            }
        } finally {
            if ($null -eq $previousIndex) { Remove-Item Env:GIT_INDEX_FILE -ErrorAction SilentlyContinue }
            else { $env:GIT_INDEX_FILE = $previousIndex }
            Remove-Item -LiteralPath $indexFile, $indexLock -Force -ErrorAction SilentlyContinue
        }
    }
    throw ("No pude publicar {0} despues de dos intentos. Git informo:`n{1}" -f $Spec.Name, $lastPushOutput)
}

function Get-RequiredExecutables {
    param([Parameter(Mandatory = $true)][string]$RequestedTarget)
    switch ($RequestedTarget) {
        "Tlalpowa" { return @("Tlalpowa.exe") }
        "Ilnamiki" { return @("Ilnamiki.exe") }
        "Organizador" { return @("Organizador.exe") }
        "Suite" { return @("Ilnamiki.exe", "Organizador.exe") }
        "Todo" { return @("Tlalpowa.exe", "Ilnamiki.exe", "Organizador.exe") }
        default { return @() }
    }
}

function Assert-Executables {
    param([Parameter(Mandatory = $true)][string]$RequestedTarget)
    $missing = @()
    foreach ($name in Get-RequiredExecutables $RequestedTarget) {
        if (-not (Test-Path -LiteralPath (Join-Path $script:Root $name) -PathType Leaf)) { $missing += $name }
    }
    if ($missing.Count -gt 0) {
        throw ("Faltan ejecutables requeridos para {0}: {1}. Usa /compilar si deseas regenerarlos." -f $RequestedTarget, ($missing -join ", "))
    }
    Write-Log ("[OK] Ejecutables presentes para {0}." -f $RequestedTarget)
}

function Invoke-BuildIfRequested {
    param([Parameter(Mandatory = $true)][string]$RequestedTarget)
    if ($NoCompile -or -not $Compile) {
        Write-Log "[OK] Compilacion omitida; se publican los ejecutables actuales."
        Assert-Executables $RequestedTarget
        return
    }
    $compiler = Join-Path $script:Root "compilepushpull.cmd"
    if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { throw "No se encontro compilepushpull.cmd." }
    $buildTarget = $RequestedTarget
    if ($buildTarget -eq "Suite") { $buildTarget = "Todo" }
    Write-Section ("Compilacion explicita {0}" -f $buildTarget)
    $result = Invoke-External -FilePath $compiler -Arguments @("compilar", $buildTarget) -WorkingDirectory $script:Root -AllowFailure
    if ($result.ExitCode -ne 0) { throw ("La compilacion termino con codigo {0}." -f $result.ExitCode) }
    Assert-Executables $RequestedTarget
}

function Invoke-LoginCheck {
    Write-Section "GitHub"
    $gh = Get-Command gh.exe -ErrorAction SilentlyContinue
    if ($gh) {
        $ghPath = if ($gh.Source) { $gh.Source } else { $gh.Path }
        Invoke-External -FilePath $ghPath -Arguments @("auth", "status") -WorkingDirectory $script:Root -AllowFailure | Out-Null
    } else {
        Write-Log "[INFO] gh.exe no esta instalado; se validara el acceso Git configurado."
    }
    foreach ($name in @("Tlalpowa", "MiausoftSuite")) {
        $entry = $script:Config.repositories.$Name
        if (-not $entry) { continue }
        $result = Invoke-Git -Arguments @("ls-remote", "--exit-code", [string]$entry.url, ("refs/heads/{0}" -f [string]$entry.branch)) -AllowFailure
        if ($result.ExitCode -eq 0) { Write-Log ("[OK] {0} es accesible." -f $name) }
        else { throw ("No se pudo acceder a {0}." -f $name) }
    }
}

$mutex = New-Object System.Threading.Mutex($false, "MiausoftSuite.Publisher")
$ownsMutex = $false
try {
    try {
        $ownsMutex = $mutex.WaitOne([TimeSpan]::FromMinutes(30))
    } catch [System.Threading.AbandonedMutexException] {
        $ownsMutex = $true
    }
    if (-not $ownsMutex) { throw "Hay otra publicacion de MiausoftSuite en curso." }

    $script:GitExe = Resolve-Git
    $script:Config = Read-RepositoryConfig
    Write-Section "Publicador MiausoftSuite"
    Write-Log ("Raiz fuente: {0}" -f $script:Root)
    Write-Log ("Registro: {0}" -f $script:LogPath)
    Write-Log "[OK] La raiz fuente no necesita carpeta .git; se usa un indice tecnico aislado."

    if ($Login) {
        Invoke-LoginCheck
        exit 0
    }
    if (-not $Target) { throw "No se indico Target." }

    Invoke-BuildIfRequested $Target
    $specs = @(Get-SpecsForTarget $Target)
    foreach ($spec in $specs) { Publish-Spec $spec }

    Write-Section "Resultado"
    if ($DryRun) {
        Write-Log "[OK] Simulacion finalizada: contenido, commit candidato y permiso de push validados; GitHub no fue modificado."
    } else {
        Write-Log "[OK] Publicacion finalizada y verificada contra origin."
    }
    exit 0
} catch {
    Write-Section "ERROR"
    Write-Log $_.Exception.Message
    Write-Log ("Registro: {0}" -f $script:LogPath)
    exit 1
} finally {
    if ($ownsMutex) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
}
