function Install-GasairBundle {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$Source,
        [Parameter(Mandatory=$true)][string]$DestinationRoot,
        [Parameter(Mandatory=$true)][string]$DataRoot
    )
    $ErrorActionPreference = 'Stop'
    $sourcePath = [IO.Path]::GetFullPath($Source)
    $rootPath = [IO.Path]::GetFullPath($DestinationRoot).TrimEnd('\')
    $dataPath = [IO.Path]::GetFullPath($DataRoot).TrimEnd('\')
    $target = [IO.Path]::GetFullPath((Join-Path $rootPath 'GASAIR.vst3'))
    if ([IO.Path]::GetDirectoryName($target) -ne $rootPath) { throw 'Invalid installation destination.' }
    if ($sourcePath -eq $target -or $dataPath.StartsWith($target + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid source or backup destination.' }
    $relativeBinary = 'Contents\x86_64-win\GASAIR.vst3'
    if (-not (Test-Path -LiteralPath (Join-Path $sourcePath $relativeBinary) -PathType Leaf)) { throw 'Plugin files are missing. Extract the complete ZIP first.' }
    # A locked DLL indicates a running host. Stop before moving the old bundle.
    if (Test-Path -LiteralPath $target) {
        $existing = if (Test-Path -LiteralPath $target -PathType Container) { Join-Path $target $relativeBinary } else { $target }
        if (Test-Path -LiteralPath $existing -PathType Leaf) {
            try { $handle = [IO.File]::Open($existing, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None); $handle.Dispose() }
            catch { throw 'GASAIR is in use or not writable. Completely close your DAW, then run the installer again.' }
        }
    }
    $runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
    $stageRoot = Join-Path $dataPath ('InstallStaging\' + $runId)
    $stage = [IO.Path]::GetFullPath((Join-Path $stageRoot 'GASAIR.vst3'))
    $backupRoot = Join-Path $dataPath ('Backups\' + $runId)
    $backup = [IO.Path]::GetFullPath((Join-Path $backupRoot 'GASAIR.vst3'))
    foreach ($path in @($stage, $backup)) {
        if (-not $path.StartsWith($dataPath + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid staging or backup path.' }
    }
    New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $stage -Recurse
    foreach ($file in Get-ChildItem -LiteralPath $sourcePath -Recurse -File) {
        $relative = $file.FullName.Substring($sourcePath.TrimEnd('\').Length + 1)
        $copied = Join-Path $stage $relative
        if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $copied).Hash) { throw ('Copy verification failed: ' + $relative) }
    }
    New-Item -ItemType Directory -Path $rootPath -Force | Out-Null
    $backedUp = $false
    try {
        if (Test-Path -LiteralPath $target) {
            New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
            Move-Item -LiteralPath $target -Destination $backup
            $backedUp = $true
        }
        Move-Item -LiteralPath $stage -Destination $target
    } catch {
        if ($backedUp -and -not (Test-Path -LiteralPath $target)) {
            Move-Item -LiteralPath $backup -Destination $target
        }
        throw
    }
    Write-Output 'GASAIR 1.0.3 by Gassymixing installed successfully.'
    Write-Output ('Installed to: ' + $target)
    if ($backedUp) { Write-Output ('Previous version backed up to: ' + $backup) }
    Write-Output 'Restart your DAW and rescan VST3 plugins.'
}
