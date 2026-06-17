[CmdletBinding()]
param(
    [switch]$NoPause,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2.0

$suiteRoot = Split-Path -Parent $PSScriptRoot
$externalDriveRoot = "D:\MiausoftSuite"
$remoteUrl = "https://github.com/mauricioisbl/MiausoftSuite.git"
$branch = "main"

function Write-Step {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host $Message
}

function Convert-ByteCountToKilobytes {
    param([Parameter(Mandatory = $true)][uint64]$ByteCount)
    if ($ByteCount -eq 0) { return [uint64]0 }
    return [uint64][Math]::Ceiling(([double]$ByteCount) / 1024.0)
}

function Get-FileKilobytesOrNull {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    $item = Get-Item -LiteralPath $Path -Force
    return (Convert-ByteCountToKilobytes -ByteCount ([uint64]$item.Length))
}

function Format-KBValue {
    param($Value)
    if ($null -eq $Value) { return "ausente" }
    return ("{0} KB" -f $Value)
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
    $safe = (Normalize-RelativePath -Path $RelativePath).Replace("/", [System.IO.Path]::DirectorySeparatorChar)
    return [System.IO.Path]::GetFullPath((Join-Path $Root $safe))
}

function Resolve-Git {
    $command = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $(if ($command.Source) { $command.Source } else { $command.Path })
    }
    $fallback = "C:\Program Files\Git\cmd\git.exe"
    if (Test-Path -LiteralPath $fallback -PathType Leaf) { return $fallback }
    throw "No se encontro git.exe."
}

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [switch]$Quiet,
        [switch]$AllowFailure
    )
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = & $script:GitExe -C $suiteRoot -c core.longpaths=true -c gc.auto=0 -c maintenance.auto=false @Arguments 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }
    if (-not $Quiet) {
        foreach ($line in @($output)) {
            if (-not [string]::IsNullOrWhiteSpace([string]$line)) { Write-Host $line }
        }
    }
    if ($exitCode -ne 0 -and -not $AllowFailure) {
        throw ("git {0} fallo con codigo {1}. {2}" -f ($Arguments -join " "), $exitCode, (@($output) -join "`n"))
    }
    return [pscustomobject]@{ ExitCode = $exitCode; Output = @($output) }
}

function Get-CurrentCommitOrNull {
    $result = Invoke-Git -Arguments @("rev-parse", "HEAD") -Quiet -AllowFailure
    if ($result.ExitCode -ne 0 -or $result.Output.Count -eq 0) { return $null }
    return ([string]$result.Output[0]).Trim()
}

function Parse-GitNameStatus {
    param([Parameter(Mandatory = $true)]$Lines)
    $items = New-Object 'System.Collections.Generic.List[object]'
    foreach ($line in @($Lines)) {
        if ([string]::IsNullOrWhiteSpace([string]$line)) { continue }
        $parts = ([string]$line).Split("`t")
        if ($parts.Count -lt 2) { continue }
        $status = $parts[0]
        $oldPath = Normalize-RelativePath -Path $parts[1]
        $newPath = $oldPath
        if (($status.StartsWith("R", [System.StringComparison]::OrdinalIgnoreCase) -or
             $status.StartsWith("C", [System.StringComparison]::OrdinalIgnoreCase)) -and $parts.Count -ge 3) {
            $newPath = Normalize-RelativePath -Path $parts[2]
        }
        $items.Add([pscustomobject]@{
            Status = $status
            OldPath = $oldPath
            NewPath = $newPath
        }) | Out-Null
    }
    return $items.ToArray()
}

function Sync-PullChangesToExternalD {
    param([Parameter(Mandatory = $true)]$Changes)

    $synced = New-Object 'System.Collections.Generic.List[object]'
    $suiteDrive = [System.IO.Path]::GetPathRoot([System.IO.Path]::GetFullPath($suiteRoot))
    if ($suiteDrive.Equals("D:\", [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Step "[OK] Copia D omitida: la suite ya se esta ejecutando desde D:."
        return $synced.ToArray()
    }
    if (-not (Test-Path -LiteralPath "D:\" -PathType Container)) {
        Write-Step "[OK] Copia D omitida: D: no esta disponible."
        return $synced.ToArray()
    }

    New-Item -ItemType Directory -Force -Path $externalDriveRoot | Out-Null
    foreach ($change in $Changes) {
        if ($change.Status.StartsWith("R", [System.StringComparison]::OrdinalIgnoreCase) -and
            -not $change.OldPath.Equals($change.NewPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            $oldDestination = Join-RootRelative -Root $externalDriveRoot -RelativePath $change.OldPath
            if (Test-Path -LiteralPath $oldDestination -PathType Leaf) {
                $oldKB = Get-FileKilobytesOrNull -Path $oldDestination
                Remove-Item -LiteralPath $oldDestination -Force
                $synced.Add([pscustomobject]@{
                    Action = "eliminado por rename"
                    RelativePath = $change.OldPath
                    PreviousKB = $oldKB
                    CurrentKB = $null
                }) | Out-Null
            }
        }

        if ($change.Status.StartsWith("D", [System.StringComparison]::OrdinalIgnoreCase)) {
            $destination = Join-RootRelative -Root $externalDriveRoot -RelativePath $change.OldPath
            if (Test-Path -LiteralPath $destination -PathType Leaf) {
                $oldKB = Get-FileKilobytesOrNull -Path $destination
                Remove-Item -LiteralPath $destination -Force
                $synced.Add([pscustomobject]@{
                    Action = "eliminado"
                    RelativePath = $change.OldPath
                    PreviousKB = $oldKB
                    CurrentKB = $null
                }) | Out-Null
            }
            continue
        }

        $source = Join-RootRelative -Root $suiteRoot -RelativePath $change.NewPath
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { continue }
        $destination = Join-RootRelative -Root $externalDriveRoot -RelativePath $change.NewPath
        $sourceKB = Get-FileKilobytesOrNull -Path $source
        $destinationKB = Get-FileKilobytesOrNull -Path $destination
        if ($null -eq $destinationKB -or $sourceKB -ne $destinationKB) {
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
            [System.IO.File]::Copy($source, $destination, $true)
            $synced.Add([pscustomobject]@{
                Action = if ($change.Status.StartsWith("A", [System.StringComparison]::OrdinalIgnoreCase)) { "agregado" } else { "copiado" }
                RelativePath = $change.NewPath
                PreviousKB = $destinationKB
                CurrentKB = $sourceKB
            }) | Out-Null
        }
    }
    return $synced.ToArray()
}

try {
    $script:GitExe = Resolve-Git
    if (-not (Test-Path -LiteralPath (Join-Path $suiteRoot ".git") -PathType Container)) {
        throw "Esta carpeta no es un clon Git: $suiteRoot"
    }

    Write-Step "Configurando remoto MiausoftSuite..."
    $remote = Invoke-Git -Arguments @("remote", "get-url", "origin") -Quiet -AllowFailure
    if ($DryRun) {
        if ($remote.ExitCode -ne 0) {
            Write-Step "[SIMULACION] No hay remoto origin configurado; no se modificara en simulacion."
        } else {
            $remoteText = (($remote.Output | ForEach-Object { [string]$_ }) -join "").Trim()
            Write-Step "[SIMULACION] Origin actual: $remoteText"
        }
    } elseif ($remote.ExitCode -ne 0) {
        Invoke-Git -Arguments @("remote", "add", "origin", $remoteUrl) | Out-Null
    } else {
        Invoke-Git -Arguments @("remote", "set-url", "origin", $remoteUrl) | Out-Null
    }

    $before = Get-CurrentCommitOrNull
    Write-Step $(if ($DryRun) { "Simulando pull de MiausoftSuite desde GitHub..." } else { "Jalando MiausoftSuite desde GitHub..." })
    Invoke-Git -Arguments @("fetch", "--prune", "origin", $branch) | Out-Null
    if ($DryRun) {
        $remoteHead = Invoke-Git -Arguments @("rev-parse", "--verify", "origin/$branch") -Quiet
        $after = ([string]$remoteHead.Output[0]).Trim()
    } else {
        Invoke-Git -Arguments @("pull", "--rebase", "--autostash", "origin", $branch) | Out-Null
        Invoke-Git -Arguments @("submodule", "update", "--init", "--recursive") | Out-Null
        $after = Get-CurrentCommitOrNull
    }

    $changes = @()
    if ($before -and $after -and -not $before.Equals($after, [System.StringComparison]::OrdinalIgnoreCase)) {
        $diff = Invoke-Git -Arguments @("diff", "--name-status", "--find-renames", $before, $after) -Quiet
        $changes = @(Parse-GitNameStatus -Lines $diff.Output)
    }

    if ($changes.Count -eq 0) {
        Write-Step $(if ($DryRun) { "[OK] Simulacion de pull: no hay archivos cambiados o eliminados contra origin/$branch." } else { "[OK] Pull completado: no hubo archivos cambiados o eliminados desde el commit anterior." })
    } else {
        Write-Step ("[LISTA] Pull detecto {0} archivo(s) cambiado(s), eliminado(s) o renombrado(s):" -f $changes.Count)
        $sortedChanges = @($changes | Sort-Object NewPath, OldPath)
        $limit = 40
        foreach ($change in ($sortedChanges | Select-Object -First $limit)) {
            $pathText = if ($change.OldPath.Equals($change.NewPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                $change.NewPath
            } else {
                "$($change.OldPath) -> $($change.NewPath)"
            }
            Write-Step ("  {0}`t{1}" -f $change.Status, $pathText)
        }
        if ($sortedChanges.Count -gt $limit) {
            Write-Step ("  ... {0} cambio(s) mas detectado(s)." -f ($sortedChanges.Count - $limit))
        }
    }

    if ($DryRun) {
        Write-Step "[SIMULACION] Sincronizacion de D:\MiausoftSuite omitida."
        $synced = @()
    } else {
        $synced = @(Sync-PullChangesToExternalD -Changes $changes)
    }
    if ($synced.Count -gt 0) {
        Write-Step ("[LISTA] D:\MiausoftSuite sincronizo {0} cambio(s):" -f $synced.Count)
        $sortedSynced = @($synced | Sort-Object RelativePath, Action)
        $limit = 40
        foreach ($item in ($sortedSynced | Select-Object -First $limit)) {
            Write-Step ("  {0}: {1} ({2} -> {3})" -f $item.Action, $item.RelativePath, (Format-KBValue $item.PreviousKB), (Format-KBValue $item.CurrentKB))
        }
        if ($sortedSynced.Count -gt $limit) {
            Write-Step ("  ... {0} cambio(s) mas sincronizado(s)." -f ($sortedSynced.Count - $limit))
        }
    }

    Write-Step $(if ($DryRun) { "[OK] Simulacion de pull completada." } else { "[OK] MiausoftSuite quedo actualizada." })
    exit 0
} catch {
    Write-Host ""
    Write-Host "[ERROR] No pude completar el pull." -ForegroundColor Red
    Write-Host $_.Exception.Message
    exit 1
}
