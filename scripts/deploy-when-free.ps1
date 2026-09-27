$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$staged = Join-Path $root 'dist\OTEditor.next.exe'
$target = Join-Path $root 'dist\OTEditor.exe'
$log = Join-Path $root 'build\deferred-deploy.log'

"Waiting for OTEditor to close" | Set-Content -LiteralPath $log
while (Get-Process OTEditor -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $target }) {
    Start-Sleep -Seconds 5
}
for ($attempt = 0; $attempt -lt 300; $attempt++) {
    try {
        Copy-Item -LiteralPath $staged -Destination $target -Force -ErrorAction Stop
        $expected = (Get-FileHash -LiteralPath $staged -Algorithm SHA256).Hash
        $installed = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        if ($expected -eq $installed) {
            Remove-Item -LiteralPath $staged -Force
            "Installed $installed at $(Get-Date -Format o)" | Set-Content -LiteralPath $log
            exit 0
        }
    } catch {
        "Attempt $attempt`: $($_.Exception.Message)" | Set-Content -LiteralPath $log
    }
    Start-Sleep -Seconds 2
}
"Could not replace OTEditor.exe after the old process exited" | Set-Content -LiteralPath $log
exit 1
