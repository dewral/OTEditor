param([switch]$Deploy)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$qt = if ($env:QT_ROOT) { $env:QT_ROOT } else { 'C:\Qt\6.10.2\mingw_64' }
$env:PATH = "$qt\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;$env:PATH"
cmake -S $root -B "$root\build" -G Ninja "-DCMAKE_PREFIX_PATH=$qt" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
if ($LASTEXITCODE) { exit $LASTEXITCODE }
cmake --build "$root\build" --parallel 6
if ($LASTEXITCODE) { exit $LASTEXITCODE }
ctest --test-dir "$root\build" --output-on-failure
if ($LASTEXITCODE) { exit $LASTEXITCODE }
if ($Deploy) {
    New-Item -ItemType Directory -Force "$root\dist" | Out-Null
    $target = Join-Path $root 'dist\OTEditor.exe'
    $staged = Join-Path $root 'dist\OTEditor.next.exe'
    $source = Join-Path $root 'build\bin\OTEditor.exe'
    $running = Get-Process OTEditor -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $target }
    if ($running) {
        Copy-Item -LiteralPath $source -Destination $staged -Force
        & "$qt\bin\windeployqt.exe" --release --qmldir "$root\qml" $staged
        if ($LASTEXITCODE) { exit $LASTEXITCODE }
        $helper = Join-Path $PSScriptRoot 'deploy-when-free.ps1'
        $pidFile = Join-Path $root 'build\deferred-deploy.pid'
        $existingId = if (Test-Path -LiteralPath $pidFile) { [int](Get-Content -LiteralPath $pidFile -Raw) } else { 0 }
        if (!$existingId -or !(Get-Process -Id $existingId -ErrorAction SilentlyContinue)) {
            $deferred = Start-Process -FilePath 'powershell.exe' -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$helper`"" -WindowStyle Hidden -PassThru
            $deferred.Id | Set-Content -LiteralPath $pidFile
        }
        Write-Host 'OTEditor is running. The new EXE is staged and will replace it after the app closes.'
        exit 0
    }
    Copy-Item -LiteralPath $source -Destination $target -Force
    & "$qt\bin\windeployqt.exe" --release --qmldir "$root\qml" $target
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
}
