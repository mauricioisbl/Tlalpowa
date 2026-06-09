@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul 2>nul
title Tlalpowa - Publicar
for %%I in ("%~dp0.") do set "TLALPOHUA_BASE=%%~fI"
set "TLALPOHUA_SELF=%~f0"
set "TLALPOHUA_PS1=%TEMP%\tlalpohua_publicar_tlalpowa_%RANDOM%%RANDOM%%RANDOM%.ps1"
set "TLALPOHUA_NO_PAUSE=0"
set "TLALPOHUA_ARGS_SCAN=%*"
echo %TLALPOHUA_ARGS_SCAN% | findstr /i /c:"/no-pause" /c:"--no-pause" /c:"-no-pause" /c:"/nopause" /c:"--nopause" /c:"-nopause" >nul 2>nul
if not errorlevel 1 set "TLALPOHUA_NO_PAUSE=1"
set "TLALPOHUA_LOG=%TLALPOHUA_BASE%\PRETLALPOWA.LOG"
> "%TLALPOHUA_LOG%" echo [BOOT] Publicar.cmd
>>"%TLALPOHUA_LOG%" echo [BOOT] Fecha/Hora CMD: %DATE% %TIME%
>>"%TLALPOHUA_LOG%" echo [BOOT] Script: %TLALPOHUA_SELF%
>>"%TLALPOHUA_LOG%" echo [BOOT] Argumentos: %*
>>"%TLALPOHUA_LOG%" echo [BOOT] Contrato: rutas esenciales; commit incremental por indice Git; archivos de 100 MiB o mas omitidos automaticamente.
echo [LOG] %TLALPOHUA_LOG%
echo [LANZADOR] %~nx0
echo [CONTRATO] Rutas esenciales; commit incremental por indice Git; archivos de 100 MiB o mas omitidos automaticamente.
echo [ANTICOLAPSO] La ventana quedara abierta al terminar, salvo que uses /no-pause.
where powershell.exe >nul 2>nul
if errorlevel 1 (
  echo [ERROR] PowerShell no esta disponible en PATH.
  >>"%TLALPOHUA_LOG%" echo [ERROR] PowerShell no esta disponible en PATH.
  goto TLALPOHUA_FAIL_EARLY
)
for /f "usebackq delims=" %%G in (`powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "try { (Get-Command powershell.exe -ErrorAction Stop).Source } catch { 'powershell.exe' }"`) do set "TLALPOHUA_POWERSHELL_EXE=%%G"
if not defined TLALPOHUA_POWERSHELL_EXE set "TLALPOHUA_POWERSHELL_EXE=powershell.exe"
>>"%TLALPOHUA_LOG%" echo [BOOT] PowerShell: %TLALPOHUA_POWERSHELL_EXE%
"%TLALPOHUA_POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $self=$env:TLALPOHUA_SELF; $out=$env:TLALPOHUA_PS1; $log=$env:TLALPOHUA_LOG; Add-Content -LiteralPath $log -Encoding UTF8 -Value ('[BOOT] Extrayendo bloque PowerShell interno desde: ' + $self); $lines=[System.IO.File]::ReadAllLines($self,[System.Text.Encoding]::UTF8); $idx=[Array]::IndexOf($lines,'@@TLALPOHUA_POWERSHELL@@'); if($idx -lt 0){ throw 'No se encontro el marcador PowerShell interno.' }; if($idx + 1 -ge $lines.Length){ throw 'El bloque PowerShell interno esta vacio.' }; $body=$lines[($idx+1)..($lines.Length-1)]; $enc=New-Object System.Text.UTF8Encoding -ArgumentList $false; [System.IO.File]::WriteAllLines($out,$body,$enc); Add-Content -LiteralPath $log -Encoding UTF8 -Value ('[BOOT] PS1 temporal: ' + $out)"
if errorlevel 1 (
  echo [ERROR] No pude extraer el bloque PowerShell interno.
  >>"%TLALPOHUA_LOG%" echo [ERROR] No pude extraer el bloque PowerShell interno.
  if exist "%TLALPOHUA_PS1%" del /f /q "%TLALPOHUA_PS1%" >nul 2>nul
  goto TLALPOHUA_FAIL_EARLY
)
>>"%TLALPOHUA_LOG%" echo [BOOT] Invocando publicador interno.
"%TLALPOHUA_POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%TLALPOHUA_PS1%" %* --no-pause
set "TLALPOHUA_RC=%ERRORLEVEL%"
>>"%TLALPOHUA_LOG%" echo [BOOT] Codigo devuelto por PowerShell: %TLALPOHUA_RC%
del /f /q "%TLALPOHUA_PS1%" >nul 2>nul
if "%TLALPOHUA_RC%"=="0" (
  call :TLALPOHUA_DELETE_SUCCESS_LOG
  if errorlevel 1 (
    set "TLALPOHUA_RC=1"
    echo [ERROR] Publicacion termino con codigo 0, pero no pude eliminar PRETLALPOWA.LOG: %TLALPOHUA_LOG%
    echo [LOG] Conservado por fallo de limpieza: %TLALPOHUA_LOG%
    echo [FIN] Publicacion terminada con codigo 1.
    goto TLALPOHUA_FAIL
  )
  echo [LOG] PRETLALPOWA.LOG eliminado por salida 0.
  echo [FIN] Publicacion terminada con codigo 0.
  goto TLALPOHUA_OK
) else (
  echo [LOG] Conservado por fallo: %TLALPOHUA_LOG%
  echo [FIN] Publicacion terminada con codigo %TLALPOHUA_RC%.
  goto TLALPOHUA_FAIL
)
:TLALPOHUA_DELETE_SUCCESS_LOG
for /L %%L in (1,1,10) do (
  if exist "%TLALPOHUA_LOG%" del /f /q "%TLALPOHUA_LOG%" >nul 2>nul
  if exist "%TLALPOHUA_LOG%" timeout /t 1 /nobreak >nul 2>nul
)
if exist "%TLALPOHUA_LOG%" exit /b 1
exit /b 0

:TLALPOHUA_FAIL_EARLY
set "TLALPOHUA_RC=1"
echo [LOG] Conservado por fallo temprano: %TLALPOHUA_LOG%
echo [FIN] Publicacion terminada con codigo 1.
goto TLALPOHUA_FAIL
:TLALPOHUA_OK
if "%TLALPOHUA_NO_PAUSE%"=="0" (
  echo.
  echo Presiona ENTER para cerrar esta ventana.
  set /p TLALPOHUA_ENTER=
)
exit /b 0
:TLALPOHUA_FAIL
if "%TLALPOHUA_NO_PAUSE%"=="0" (
  echo.
  echo Presiona ENTER para cerrar esta ventana.
  set /p TLALPOHUA_ENTER=
)
exit /b %TLALPOHUA_RC%
@@TLALPOHUA_POWERSHELL@@
$ErrorActionPreference = 'Stop'
Set-StrictMode -Off
$script:Version = '2026-06-09-indice-sin-checkout-nombres-finales'
$script:GitExe = $null
$script:LogPath = $env:TLALPOHUA_LOG
$script:TranscriptStarted = $false
$script:PublishRoot = $null
$script:LimitBytes = 100MB
function Write-Section([string]$Text) {
    Write-Host ''
    Write-Host '============================================================'
    Write-Host (' ' + $Text)
    Write-Host '============================================================'
}
function Write-Ok([string]$Text) { Write-Host ('[OK] ' + $Text) }
function Write-Warn([string]$Text) { Write-Host ('[ADVERTENCIA] ' + $Text) -ForegroundColor Yellow }
function Write-Err([string]$Text) { Write-Host ('[ERROR] ' + $Text) -ForegroundColor Red }
function ConvertTo-MaskedRemote([string]$Remote) {
    if ([string]::IsNullOrWhiteSpace($Remote)) { return '' }
    return ($Remote -replace '^(https?://)([^/@:]+:)?([^/@]+)@', '$1***@')
}
function Get-RelativePath([string]$Root, [string]$FullName) {
    $r = [System.IO.Path]::GetFullPath($Root).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $f = [System.IO.Path]::GetFullPath($FullName)
    if ($f.Length -le $r.Length) { return '' }
    return $f.Substring($r.Length + 1).Replace('\', '/')
}
function New-DirectoryStrict([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}
function Get-FirstRemoteSha([string]$Text) {
    if ([string]::IsNullOrWhiteSpace($Text)) { return $null }
    $lines = [System.Text.RegularExpressions.Regex]::Split([string]$Text, "`r?`n")
    foreach ($line in $lines) {
        $t = $line.Trim()
        if ($t.Length -eq 0) { continue }
        $m = [System.Text.RegularExpressions.Regex]::Match($t, '^[0-9a-fA-F]{40}')
        if ($m.Success) { return $m.Value.ToLowerInvariant() }
    }
    return $null
}
function Invoke-Native {
    param([string]$Exe, [string[]]$ArgumentList, [string]$WorkingDirectory, [switch]$AllowFailure, [switch]$Quiet)
    $old = (Get-Location).Path
    $code = 0
    try {
        if (-not [string]::IsNullOrWhiteSpace($WorkingDirectory)) { Set-Location -LiteralPath $WorkingDirectory }
        if (-not $Quiet) { Write-Host ('> ' + $Exe + ' ' + ($ArgumentList -join ' ')) }
        & $Exe @ArgumentList
        $code = $LASTEXITCODE
    }
    finally {
        Set-Location -LiteralPath $old
    }
    if ($null -eq $code) { $code = 0 }
    if ([int]$code -ne 0 -and -not $AllowFailure) { throw ('Fallo comando externo con codigo ' + [int]$code + ': ' + $Exe + ' ' + ($ArgumentList -join ' ')) }
    return [int]$code
}
function Invoke-NativeCapture {
    param([string]$Exe, [string[]]$ArgumentList, [string]$WorkingDirectory)
    $old = (Get-Location).Path
    $code = 0
    try {
        if (-not [string]::IsNullOrWhiteSpace($WorkingDirectory)) { Set-Location -LiteralPath $WorkingDirectory }
        $output = & $Exe @ArgumentList 2>&1
        $code = $LASTEXITCODE
    }
    finally {
        Set-Location -LiteralPath $old
    }
    if ($null -eq $code) { $code = 0 }
    return [pscustomobject]@{ Code = [int]$code; Output = ($output -join [Environment]::NewLine) }
}
function Invoke-Git([string[]]$ArgumentList, [string]$WorkingDirectory, [switch]$AllowFailure, [switch]$Quiet) {
    Invoke-Native -Exe $script:GitExe -ArgumentList $ArgumentList -WorkingDirectory $WorkingDirectory -AllowFailure:$AllowFailure -Quiet:$Quiet | Out-Null
}
function Invoke-GitCapture([string[]]$ArgumentList, [string]$WorkingDirectory) {
    return Invoke-NativeCapture -Exe $script:GitExe -ArgumentList $ArgumentList -WorkingDirectory $WorkingDirectory
}
function Invoke-RobocopyStrict([string]$Source, [string]$Destination, [string[]]$ExtraArgs) {
    $robocopy = (Get-Command robocopy.exe -ErrorAction SilentlyContinue)
    if ($null -eq $robocopy) { throw 'robocopy.exe no esta disponible.' }
    New-DirectoryStrict $Destination
    $args = New-Object System.Collections.Generic.List[string]
    $args.Add($Source)
    $args.Add($Destination)
    foreach ($x in $ExtraArgs) { $args.Add($x) }
    $old = (Get-Location).Path
    try {
        Write-Host ('> ' + $robocopy.Source + ' ' + (($args.ToArray()) -join ' '))
        & $robocopy.Source @($args.ToArray())
        $code = $LASTEXITCODE
    }
    finally {
        Set-Location -LiteralPath $old
    }
    if ($null -eq $code) { $code = 0 }
    if ([int]$code -ge 8) { throw ('robocopy fallo con codigo ' + [int]$code + ': ' + $Source + ' -> ' + $Destination) }
    return [int]$code
}
function New-ShortTempBase {
    $candidates = @($env:TLALPOHUA_TEMP, 'C:\TPW')
    if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) { $candidates += (Join-Path $env:LOCALAPPDATA 'Temp\TPW') }
    if (-not [string]::IsNullOrWhiteSpace($env:TEMP)) { $candidates += (Join-Path $env:TEMP 'TPW') }
    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        try {
            $full = [System.IO.Path]::GetFullPath($candidate)
            New-DirectoryStrict $full
            $probe = Join-Path $full ('.probe_' + $PID + '_' + [System.Guid]::NewGuid().ToString('N'))
            [System.IO.File]::WriteAllText($probe, 'ok')
            Remove-Item -LiteralPath $probe -Force -ErrorAction SilentlyContinue
            return (Resolve-Path -LiteralPath $full).Path
        }
        catch {
        }
    }
    throw 'No encontre una carpeta temporal escribible.'
}
function Read-NextArgument([string[]]$AllArgs, [ref]$Index, [string]$Name) {
    if ($Index.Value + 1 -ge $AllArgs.Count) { throw ('El argumento ' + $Name + ' requiere un valor.') }
    $Index.Value = $Index.Value + 1
    return [string]$AllArgs[$Index.Value]
}
function Show-Usage {
    Write-Host 'Uso:'
    Write-Host '  Publicar.cmd [opciones]'
    Write-Host 'Opciones:'
    Write-Host '  /remote URL'
    Write-Host '  /branch RAMA'
    Write-Host '  /email CORREO'
    Write-Host '  /message TEXTO'
    Write-Host '  /dry-run'
    Write-Host '  /no-push'
    Write-Host '  /keep-temp'
    Write-Host '  /keep-log'
    Write-Host '  /no-pause'
}
function Get-GitExe {
    $cmd = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($null -ne $cmd) { return $cmd.Source }
    $fallback = Join-Path $env:ProgramFiles 'Git\cmd\git.exe'
    if (Test-Path -LiteralPath $fallback -PathType Leaf) { return $fallback }
    throw 'git.exe no esta disponible en PATH.'
}
function Get-SourceFilesForManifest([string]$Base, [object[]]$Manifest) {
    $items = New-Object System.Collections.Generic.List[object]
    foreach ($m in $Manifest) {
        if (-not (Test-Path -LiteralPath $m.Source)) { throw ('No existe la ruta requerida: ' + $m.Source) }
        if (Test-Path -LiteralPath $m.Source -PathType Leaf) {
            $fi = Get-Item -LiteralPath $m.Source -Force -ErrorAction Stop
            $items.Add([pscustomobject]@{ FullName = $fi.FullName; Length = [int64]$fi.Length; Relative = $m.Target })
        }
        else {
            $files = @(Get-ChildItem -LiteralPath $m.Source -Recurse -Force -File -ErrorAction Stop)
            foreach ($fi in $files) {
                $relInside = Get-RelativePath -Root $m.Source -FullName $fi.FullName
                $items.Add([pscustomobject]@{ FullName = $fi.FullName; Length = [int64]$fi.Length; Relative = ($m.Target + '/' + $relInside) })
            }
        }
    }
    return @($items.ToArray())
}
function Copy-ManifestToStage([object[]]$Manifest, [string]$Stage) {
    $maxAllowed = [int64]($script:LimitBytes - 1)
    New-DirectoryStrict $Stage
    foreach ($m in $Manifest) {
        if (Test-Path -LiteralPath $m.Source -PathType Leaf) {
            $fi = Get-Item -LiteralPath $m.Source -Force -ErrorAction Stop
            if ([int64]$fi.Length -lt [int64]$script:LimitBytes) {
                $dst = Join-Path $Stage $m.Target
                $parent = Split-Path -Parent $dst
                New-DirectoryStrict $parent
                Copy-Item -LiteralPath $fi.FullName -Destination $dst -Force -ErrorAction Stop
            }
        }
        else {
            $dst = Join-Path $Stage $m.Target
            $args = @('/E', '/COPY:DAT', '/DCOPY:DAT', '/R:2', '/W:1', '/NFL', '/NDL', '/NP', '/NJH', '/NJS', '/XD', '.git', '/MAX:' + $maxAllowed)
            Invoke-RobocopyStrict -Source $m.Source -Destination $dst -ExtraArgs $args | Out-Null
        }
    }
}
function Remove-TooLargeFilesFromStage([string]$Stage) {
    $removed = New-Object System.Collections.Generic.List[object]
    $big = @(Get-ChildItem -LiteralPath $Stage -Recurse -Force -File -ErrorAction Stop | Where-Object { [int64]$_.Length -ge [int64]$script:LimitBytes })
    foreach ($fi in $big) {
        $rel = Get-RelativePath -Root $Stage -FullName $fi.FullName
        $removed.Add([pscustomobject]@{ Relative = $rel; Length = [int64]$fi.Length })
        Remove-Item -LiteralPath $fi.FullName -Force -ErrorAction Stop
    }
    return @($removed.ToArray())
}
function Format-MiB([int64]$Bytes) {
    return ('{0:N2} MiB' -f ($Bytes / 1MB))
}
function Assert-ManifestUnderBase([string]$Base, [object[]]$Manifest) {
    $baseFull = [System.IO.Path]::GetFullPath($Base).TrimEnd('\')
    foreach ($m in $Manifest) {
        $full = [System.IO.Path]::GetFullPath($m.Source)
        if (-not ($full.Equals($baseFull, [System.StringComparison]::OrdinalIgnoreCase) -or $full.StartsWith($baseFull + '\', [System.StringComparison]::OrdinalIgnoreCase))) {
            throw ('Ruta fuera de C:\Tlalpowa bloqueada: ' + $full)
        }
    }
}
function Copy-StageIntoRepo([string]$Stage, [string]$Repo) {
    $args = @('/E', '/COPY:DAT', '/DCOPY:DAT', '/R:2', '/W:1', '/NFL', '/NDL', '/NP', '/NJH', '/NJS')
    Invoke-RobocopyStrict -Source $Stage -Destination $Repo -ExtraArgs $args | Out-Null
}
function Add-TargetPathsToGit([string]$Repo, [string[]]$Targets) {
    $rm = New-Object System.Collections.Generic.List[string]
    foreach ($x in @('rm', '-r', '--cached', '--ignore-unmatch', '--quiet', '--')) { $rm.Add($x) }
    foreach ($t in $Targets) { $rm.Add($t) }
    Invoke-Git -ArgumentList ([string[]]$rm.ToArray()) -WorkingDirectory $Repo
    $add = New-Object System.Collections.Generic.List[string]
    foreach ($x in @('-c', 'core.longpaths=true', 'add', '-A', '--force', '--')) { $add.Add($x) }
    foreach ($t in $Targets) { $add.Add($t) }
    Invoke-Git -ArgumentList ([string[]]$add.ToArray()) -WorkingDirectory $Repo
}
function Ensure-RemoteUnchanged([string]$Repo, [string]$Branch, [string]$ExpectedSha) {
    $probe = Invoke-GitCapture -ArgumentList @('ls-remote', '--heads', 'origin', $Branch) -WorkingDirectory $Repo
    if ($probe.Code -ne 0) { throw 'No pude verificar la rama remota antes del push.' }
    $now = Get-FirstRemoteSha -Text $probe.Output
    if ([string]::IsNullOrWhiteSpace($ExpectedSha)) {
        if (-not [string]::IsNullOrWhiteSpace($now)) { throw ('La rama remota fue creada durante la publicacion: ' + $Branch) }
    }
    else {
        if ([string]::IsNullOrWhiteSpace($now)) { throw ('La rama remota desaparecio durante la publicacion: ' + $Branch) }
        if ($now.ToLowerInvariant() -ne $ExpectedSha.ToLowerInvariant()) { throw ('La rama remota cambio durante la publicacion. Esperado=' + $ExpectedSha + ' Actual=' + $now) }
    }
}
function Verify-RemoteHead([string]$Repo, [string]$Branch, [string]$ExpectedHead) {
    $probe = Invoke-GitCapture -ArgumentList @('ls-remote', '--heads', 'origin', $Branch) -WorkingDirectory $Repo
    if ($probe.Code -ne 0) { throw 'No pude verificar GitHub despues del push.' }
    $sha = Get-FirstRemoteSha -Text $probe.Output
    if ([string]::IsNullOrWhiteSpace($sha)) { throw ('No encontre la rama remota despues del push: ' + $Branch) }
    if ($sha.ToLowerInvariant() -ne $ExpectedHead.ToLowerInvariant()) { throw ('GitHub no apunta al commit publicado. Local=' + $ExpectedHead + ' Remoto=' + $sha) }
    Write-Ok ('Verificacion remota exacta: refs/heads/' + $Branch + ' @ ' + $sha)
}
try {
    if (-not [string]::IsNullOrWhiteSpace($script:LogPath)) {
        try {
            Start-Transcript -Path $script:LogPath -Append | Out-Null
            $script:TranscriptStarted = $true
        }
        catch {
            $script:TranscriptStarted = $false
        }
    }
    $Remote = $env:TLALPOWA_GIT_REMOTE
    if ([string]::IsNullOrWhiteSpace($Remote)) { $Remote = 'https://github.com/mauricioisbl/Tlalpowa.git' }
    $Branch = $env:TLALPOWA_GIT_BRANCH
    if ([string]::IsNullOrWhiteSpace($Branch)) { $Branch = 'main' }
    $PublisherName = 'Tlalpohua'
    $PublisherEmail = $env:TLALPOHUA_GIT_EMAIL
    if ([string]::IsNullOrWhiteSpace($PublisherEmail)) { $PublisherEmail = $env:TLALPOWA_GIT_EMAIL }
    if ([string]::IsNullOrWhiteSpace($PublisherEmail)) { $PublisherEmail = 'tlalpohua@users.noreply.github.com' }
    $CommitMessage = $null
    $DryRun = $false
    $NoPush = $false
    $KeepTemp = $false
    $NoPause = $false
    $RemoteWasPositional = $false
    $raw = @($args)
    for ($i = 0; $i -lt $raw.Count; $i++) {
        $a = [string]$raw[$i]
        $lower = $a.ToLowerInvariant()
        if ($lower -in @('/?', '-?', '--help', '/help', '-help')) { Show-Usage; exit 0 }
        elseif ($lower -in @('/nopause', '--nopause', '-nopause', '/no-pause', '--no-pause', '-no-pause')) { $NoPause = $true }
        elseif ($lower -in @('/keep-temp', '--keep-temp', '-keep-temp', '/keeptemp', '--keeptemp', '-keeptemp')) { $KeepTemp = $true }
        elseif ($lower -in @('/dry-run', '--dry-run', '-dry-run', '/dryrun', '--dryrun', '-dryrun')) { $DryRun = $true }
        elseif ($lower -in @('/no-push', '--no-push', '-no-push', '/nopush', '--nopush', '-nopush')) { $NoPush = $true }
        elseif ($lower -in @('/remote', '--remote', '-remote')) { $Remote = Read-NextArgument -AllArgs $raw -Index ([ref]$i) -Name $a }
        elseif ($lower -in @('/branch', '--branch', '-branch')) { $Branch = Read-NextArgument -AllArgs $raw -Index ([ref]$i) -Name $a }
        elseif ($lower -in @('/email', '--email', '-email')) { $PublisherEmail = Read-NextArgument -AllArgs $raw -Index ([ref]$i) -Name $a }
        elseif ($lower -in @('/message', '--message', '-message', '/msg', '--msg', '-msg')) { $CommitMessage = Read-NextArgument -AllArgs $raw -Index ([ref]$i) -Name $a }
        elseif ($lower -in @('/replace-remote', '--replace-remote', '-replace-remote', '/force-with-lease', '--force-with-lease', '-force-with-lease', '/refuse-existing', '--refuse-existing', '-refuse-existing', '/keep-log', '--keep-log', '-keep-log', '/keeplog', '--keeplog', '-keeplog')) { }
        elseif (-not $RemoteWasPositional -and ($a -match '^(https?://|ssh://|git@)')) { $Remote = $a; $RemoteWasPositional = $true }
        else { throw ('Argumento no reconocido: ' + $a) }
    }
    $Base = 'C:\Tlalpowa'
    $Manifest = @(
        [pscustomobject]@{ Source = 'C:\Tlalpowa\Tlalpowa.exe'; Target = 'Tlalpowa.exe' },
        [pscustomobject]@{ Source = 'C:\Tlalpowa\Fuente'; Target = 'Fuente' },
        [pscustomobject]@{ Source = 'C:\Tlalpowa\Datos'; Target = 'Datos' },
        [pscustomobject]@{ Source = 'C:\Tlalpowa\Compilar.cmd'; Target = 'Compilar.cmd' },
        [pscustomobject]@{ Source = 'C:\Tlalpowa\Publicar.cmd'; Target = 'Publicar.cmd' },
        [pscustomobject]@{ Source = 'C:\Tlalpowa\Iconos.cmd'; Target = 'Iconos.cmd' }
    )
    Write-Section 'Tlalpohua - publicar Tlalpowa en GitHub'
    Write-Host ('Version:      ' + $script:Version)
    Write-Host ('GitHub:       ' + (ConvertTo-MaskedRemote $Remote))
    Write-Host ('Rama:         ' + $Branch)
    Write-Host ('Autor:        ' + $PublisherName + ' <' + $PublisherEmail + '>')
    Write-Host ('Base:         ' + $Base)
    Write-Host ('Modo seco:    ' + $DryRun)
    Write-Host ('Sin push:     ' + $NoPush)
    Write-Host 'Politica:     sin checkout del remoto; reemplazo incremental de rutas esenciales por indice Git'
    Write-Host 'Alcance:      Tlalpowa.exe, Fuente, Datos, Compilar.cmd, Publicar.cmd, Iconos.cmd; archivos >=100 MiB omitidos'
    $script:GitExe = Get-GitExe
    $gitVersion = Invoke-NativeCapture -Exe $script:GitExe -ArgumentList @('--version') -WorkingDirectory $null
    Write-Host ('Git:          ' + $gitVersion.Output.Trim())
    Invoke-Git -ArgumentList @('check-ref-format', '--branch', $Branch) -WorkingDirectory $null -Quiet
    if (-not (Test-Path -LiteralPath $Base -PathType Container)) { throw ('No existe la base requerida: ' + $Base) }
    Assert-ManifestUnderBase -Base $Base -Manifest $Manifest
    Write-Section 'Preparando stage temporal verificado'
    $tempBase = New-ShortTempBase
    $script:PublishRoot = Join-Path $tempBase ('TP' + (Get-Random -Minimum 100000 -Maximum 999999) + '_' + $PID)
    $Stage = Join-Path $script:PublishRoot 's'
    $Repo = Join-Path $script:PublishRoot 'r'
    New-DirectoryStrict $Stage
    New-DirectoryStrict $Repo
    Write-Host ('Temp:         ' + $script:PublishRoot)
    Write-Ok 'Rutas esenciales validadas bajo C:\Tlalpowa.'
    $sourceFiles = @(Get-SourceFilesForManifest -Base $Base -Manifest $Manifest)
    $omitted = @($sourceFiles | Where-Object { [int64]$_.Length -ge [int64]$script:LimitBytes })
    if ($omitted.Count -gt 0) {
        Write-Warn ('Archivos de 100 MiB o mas omitidos automaticamente del stage temporal:')
        foreach ($o in $omitted) { Write-Host ('  - ' + $o.Relative + ' = ' + (Format-MiB -Bytes $o.Length)) }
    }
    Copy-ManifestToStage -Manifest $Manifest -Stage $Stage
    $removedLate = @(Remove-TooLargeFilesFromStage -Stage $Stage)
    foreach ($r in $removedLate) { Write-Warn ('Omitido por verificacion final: ' + $r.Relative + ' = ' + (Format-MiB -Bytes $r.Length)) }
    $stageFiles = @(Get-ChildItem -LiteralPath $Stage -Recurse -Force -File -ErrorAction Stop)
    $stageDirs = @(Get-ChildItem -LiteralPath $Stage -Recurse -Force -Directory -ErrorAction SilentlyContinue)
    $stageBytes = [int64]0
    foreach ($f in $stageFiles) { $stageBytes = $stageBytes + [int64]$f.Length }
    if ($stageFiles.Count -le 0) { throw 'El stage temporal quedo sin archivos.' }
    $bad = @($stageFiles | Where-Object { [int64]$_.Length -ge [int64]$script:LimitBytes })
    if ($bad.Count -gt 0) { throw 'La verificacion final encontro archivos que exceden el limite de GitHub.' }
    Write-Ok ('Filtrado GitHub aplicado: ' + $omitted.Count + ' archivo(s) omitido(s); todo lo restante queda por debajo de 100 MiB.')
    Write-Ok ('Stage listo: ' + $Stage)
    Write-Ok ('Archivos: ' + $stageFiles.Count + '  Carpetas: ' + $stageDirs.Count + '  Tamano: ' + (Format-MiB -Bytes $stageBytes))
    if ($DryRun) {
        Write-Section 'Modo seco completado'
        Write-Ok 'No se inicializo Git, no se creo commit y no se hizo push.'
        if ($KeepTemp) { Write-Ok ('Temporal conservado: ' + $script:PublishRoot) } else { Remove-Item -LiteralPath $script:PublishRoot -Recurse -Force -ErrorAction SilentlyContinue }
        exit 0
    }
    Write-Section 'Preparando base Git remota sin checkout'
    Invoke-Git -ArgumentList @('init') -WorkingDirectory $Repo
    Invoke-Git -ArgumentList @('config', 'core.longpaths', 'true') -WorkingDirectory $Repo
    Invoke-Git -ArgumentList @('config', 'core.autocrlf', 'false') -WorkingDirectory $Repo
    Invoke-Git -ArgumentList @('config', 'core.quotepath', 'false') -WorkingDirectory $Repo
    Invoke-Git -ArgumentList @('config', 'user.name', $PublisherName) -WorkingDirectory $Repo
    Invoke-Git -ArgumentList @('config', 'user.email', $PublisherEmail) -WorkingDirectory $Repo
    Invoke-Git -ArgumentList @('remote', 'add', 'origin', $Remote) -WorkingDirectory $Repo
    $RemoteRef = 'refs/heads/' + $Branch
    $RemoteTracking = 'refs/remotes/origin/' + $Branch
    $probe = Invoke-GitCapture -ArgumentList @('ls-remote', '--heads', 'origin', $Branch) -WorkingDirectory $Repo
    if ($probe.Code -ne 0) { throw ('No pude consultar el remoto GitHub: ' + (ConvertTo-MaskedRemote $Remote)) }
    $RemoteSha = Get-FirstRemoteSha -Text $probe.Output
    Invoke-Git -ArgumentList @('symbolic-ref', 'HEAD', $RemoteRef) -WorkingDirectory $Repo
    if ([string]::IsNullOrWhiteSpace($RemoteSha)) {
        Write-Warn ('La rama remota no existe todavia: ' + $RemoteRef)
        Invoke-Git -ArgumentList @('read-tree', '--empty') -WorkingDirectory $Repo
    }
    else {
        Write-Warn ('La rama remota ya existe: ' + $RemoteRef + ' @ ' + $RemoteSha)
        Invoke-Git -ArgumentList @('fetch', '--depth', '1', 'origin', ('+' + $RemoteRef + ':' + $RemoteTracking)) -WorkingDirectory $Repo
        Invoke-Git -ArgumentList @('update-ref', $RemoteRef, $RemoteSha) -WorkingDirectory $Repo
        Invoke-Git -ArgumentList @('read-tree', 'HEAD') -WorkingDirectory $Repo
        Write-Ok 'Indice remoto cargado sin checkout de arbol de trabajo.'
    }
    Copy-StageIntoRepo -Stage $Stage -Repo $Repo
    $Targets = @('Tlalpowa.exe', 'Fuente', 'Datos', 'Compilar.cmd', 'Publicar.cmd', 'Iconos.cmd')
    Add-TargetPathsToGit -Repo $Repo -Targets $Targets
    $changes = Invoke-GitCapture -ArgumentList @('diff', '--cached', '--name-status') -WorkingDirectory $Repo
    $hasChanges = -not [string]::IsNullOrWhiteSpace($changes.Output)
    if ($hasChanges) {
        if ([string]::IsNullOrWhiteSpace($CommitMessage)) { $CommitMessage = 'Publicacion Tlalpohua ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') }
        $env:GIT_AUTHOR_NAME = $PublisherName
        $env:GIT_AUTHOR_EMAIL = $PublisherEmail
        $env:GIT_COMMITTER_NAME = $PublisherName
        $env:GIT_COMMITTER_EMAIL = $PublisherEmail
        Invoke-Git -ArgumentList @('commit', '--author', ($PublisherName + ' <' + $PublisherEmail + '>'), '-m', $CommitMessage) -WorkingDirectory $Repo
        $head = Invoke-GitCapture -ArgumentList @('rev-parse', '--verify', 'HEAD') -WorkingDirectory $Repo
        if ($head.Code -ne 0 -or [string]::IsNullOrWhiteSpace($head.Output)) { throw 'No pude leer el commit creado.' }
        $LocalHead = $head.Output.Trim()
        $tree = Invoke-GitCapture -ArgumentList @('rev-parse', 'HEAD^{tree}') -WorkingDirectory $Repo
        Write-Ok ('Commit creado: ' + $LocalHead)
        Write-Ok ('Arbol Git publicado: ' + $tree.Output.Trim())
    }
    else {
        Write-Ok 'No hay cambios nuevos que confirmar en las rutas esenciales.'
        $head = Invoke-GitCapture -ArgumentList @('rev-parse', '--verify', 'HEAD') -WorkingDirectory $Repo
        if ($head.Code -eq 0 -and -not [string]::IsNullOrWhiteSpace($head.Output)) { $LocalHead = $head.Output.Trim() } else { $LocalHead = $null }
    }
    if ($NoPush) {
        Write-Section 'Commit temporal listo sin push'
        Write-Ok ('Repositorio temporal conservado: ' + $Repo)
        $KeepTemp = $true
        exit 0
    }
    if ($hasChanges) {
        Write-Section 'Subiendo a GitHub'
        Ensure-RemoteUnchanged -Repo $Repo -Branch $Branch -ExpectedSha $RemoteSha
        Invoke-Git -ArgumentList @('push', '-u', 'origin', ('HEAD:' + $RemoteRef)) -WorkingDirectory $Repo
        Verify-RemoteHead -Repo $Repo -Branch $Branch -ExpectedHead $LocalHead
    }
    else {
        Write-Section 'GitHub sin cambios pendientes'
        if (-not [string]::IsNullOrWhiteSpace($LocalHead)) { Write-Ok ('Rama ya estaba en: ' + $LocalHead) }
    }
    Write-Section 'Publicacion completada'
    Write-Ok ('GitHub: ' + (ConvertTo-MaskedRemote $Remote))
    Write-Ok ('Rama: ' + $Branch)
    Write-Ok 'Se reemplazaron solo las rutas esenciales dentro del indice Git.'
    Write-Ok 'No se hizo checkout del remoto; los nombres imposibles de Windows no bloquean la publicacion.'
    Write-Ok 'Los archivos locales de 100 MiB o mas quedaron omitidos del commit.'
    if ($KeepTemp) {
        Write-Ok ('Temporal conservado: ' + $script:PublishRoot)
    }
    else {
        Remove-Item -LiteralPath $script:PublishRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
    if ($script:TranscriptStarted) { try { Stop-Transcript | Out-Null } catch { } }
    exit 0
}
catch {
    Write-Err $_.Exception.Message
    if ($_.ScriptStackTrace) { Write-Host $_.ScriptStackTrace -ForegroundColor DarkGray }
    if (-not $KeepTemp -and $script:PublishRoot -and (Test-Path -LiteralPath $script:PublishRoot)) {
        Remove-Item -LiteralPath $script:PublishRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
    elseif ($KeepTemp -and $script:PublishRoot) {
        Write-Warn ('Temporal conservado para diagnostico: ' + $script:PublishRoot)
    }
    if ($script:TranscriptStarted) { try { Stop-Transcript | Out-Null } catch { } }
    Write-Host ''
    Write-Host '[FALLO] Publicacion interrumpida.' -ForegroundColor Red
    exit 1
}
