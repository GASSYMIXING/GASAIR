param([switch]$Elevated)
$ErrorActionPreference = 'Stop'
$productData = Join-Path ([Environment]::GetFolderPath('CommonApplicationData')) 'Gassymixing\GASAIR'
$logPath = Join-Path $productData 'install.log'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    if ($Elevated) { Write-Host 'Administrator permission is required.'; exit 1 }
    Write-Host 'GASAIR 1.0.3 installer - please allow the Windows permission prompt.'
    try {
        $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '" -Elevated'
        $process = Start-Process -FilePath 'powershell.exe' -ArgumentList $arguments -Verb RunAs -WindowStyle Hidden -Wait -PassThru
        if (Test-Path -LiteralPath $logPath) { Get-Content -LiteralPath $logPath }
        exit $process.ExitCode
    } catch { Write-Host ('Installation cancelled or elevation failed: ' + $_.Exception.Message); exit 1 }
}
try {
    New-Item -ItemType Directory -Path $productData -Force | Out-Null
    . (Join-Path $PSScriptRoot 'Install-Core.ps1')
    $commonFiles = $env:CommonProgramW6432
    if (-not $commonFiles) { $commonFiles = [Environment]::GetFolderPath('CommonProgramFiles') }
    $result = Install-GasairBundle -Source (Join-Path $PSScriptRoot 'GASAIR.vst3') -DestinationRoot (Join-Path $commonFiles 'VST3') -DataRoot $productData
    $result | Set-Content -LiteralPath $logPath -Encoding UTF8
    $result | ForEach-Object { Write-Host $_ }
    exit 0
} catch {
    $message = 'Installation failed: ' + $_.Exception.Message
    try { $message | Set-Content -LiteralPath $logPath -Encoding UTF8 } catch {}
    Write-Host $message
    exit 1
}
