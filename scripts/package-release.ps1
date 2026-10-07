param(
    [Parameter(Mandatory)][string]$BuildBin,
    [Parameter(Mandatory)][string]$QtRoot,
    [string]$Repository = 'dewral/OTEditor',
    [string]$Version = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if (!$Version) {
    $cmake = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
    if ($cmake -notmatch 'project\(OTEditor VERSION ([0-9.]+)') { throw 'Missing application version.' }
    $Version = $Matches[1]
}
$output = Join-Path $root 'release'
$package = Join-Path $output 'OTEditor'
New-Item -ItemType Directory -Force $package | Out-Null
Copy-Item -LiteralPath (Join-Path $BuildBin 'OTEditor.exe') -Destination $package -Force
Copy-Item -LiteralPath (Join-Path $BuildBin 'OTEditorUpdater.exe') -Destination $package -Force
Get-ChildItem -LiteralPath $BuildBin -Filter '*.dll' | Copy-Item -Destination $package -Force
$deploy = Join-Path $QtRoot 'bin/windeployqt.exe'
& $deploy --release --compiler-runtime --qmldir (Join-Path $root 'qml') (Join-Path $package 'OTEditor.exe')
if ($LASTEXITCODE) { throw 'Qt application deployment failed.' }
& $deploy --release --compiler-runtime (Join-Path $package 'OTEditorUpdater.exe')
if ($LASTEXITCODE) { throw 'Qt updater deployment failed.' }
Copy-Item (Join-Path $root 'README.md') $package -Force
New-Item -ItemType Directory -Force (Join-Path $package 'assets'),(Join-Path $package 'docs/testing') | Out-Null
Copy-Item (Join-Path $root 'assets/ObjectBuilder-LICENSE.txt') (Join-Path $package 'assets') -Force
Copy-Item (Join-Path $root 'assets/screenshots') (Join-Path $package 'assets') -Recurse -Force
Copy-Item (Join-Path $root 'docs/testing/otb-compatibility.md') (Join-Path $package 'docs/testing') -Force
$archive = Join-Path $output 'OTEditor-windows-x64.zip'
Compress-Archive -Path $package -DestinationPath $archive -Force
& git -C $root archive --format=zip --output=(Join-Path $output 'OTEditor-source.zip') HEAD
if ($LASTEXITCODE) { throw 'Source archive creation failed.' }
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
$sourceHash = (Get-FileHash -LiteralPath (Join-Path $output 'OTEditor-source.zip') -Algorithm SHA256).Hash.ToLowerInvariant()
@("$hash  OTEditor-windows-x64.zip", "$sourceHash  OTEditor-source.zip") | Set-Content (Join-Path $output 'SHA256SUMS.txt') -Encoding utf8NoBOM
@{
    schemaVersion = 1
    version = $Version
    releasePageUrl = "https://github.com/$Repository/releases/tag/1.0"
    downloadUrl = "https://github.com/$Repository/releases/download/1.0/OTEditor-windows-x64.zip"
    sha256 = $hash
    size = (Get-Item -LiteralPath $archive).Length
    notes = "OTEditor $Version stable Windows release."
} | ConvertTo-Json | Set-Content (Join-Path $output 'update-manifest.json') -Encoding utf8NoBOM
