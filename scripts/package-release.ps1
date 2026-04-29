param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64')]
    [string]$Platform = 'x64',
    [string]$GameVersion = '23008567',
    [string]$ModVersion = '0.5.32'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

& (Join-Path $PSScriptRoot 'build-client.ps1') -Configuration $Configuration -Platform $Platform
& (Join-Path $PSScriptRoot 'build-server.ps1') -Configuration $Configuration -Platform $Platform

$packageName = "enshrouded-talents-presets-$GameVersion-$ModVersion"
$distRoot = Join-Path $repoRoot 'dist'
$packageRoot = Join-Path $distRoot $packageName

if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path `
    (Join-Path $packageRoot 'client\mods\talents_presets'), `
    (Join-Path $packageRoot 'server\server_root'), `
    (Join-Path $packageRoot 'optional') | Out-Null

Copy-Item -LiteralPath (Join-Path $repoRoot "build\$Platform\$Configuration\client\talents_presets.dll") -Destination (Join-Path $packageRoot 'client\mods\talents_presets\talents_presets.dll')
Copy-Item -LiteralPath (Join-Path $repoRoot 'client\talents_presets\mod.json') -Destination (Join-Path $packageRoot 'client\mods\talents_presets\mod.json')
Copy-Item -LiteralPath (Join-Path $repoRoot 'client\talents_presets\talent_presets_lang.example.txt') -Destination (Join-Path $packageRoot 'client\mods\talents_presets\talent_presets_lang.txt')
Copy-Item -LiteralPath (Join-Path $repoRoot "build\$Platform\$Configuration\server\dbghelp.dll") -Destination (Join-Path $packageRoot 'server\server_root\dbghelp.dll')
Copy-Item -LiteralPath (Join-Path $repoRoot 'examples\talent_presets.example.txt') -Destination (Join-Path $packageRoot 'optional\talent_presets_example.txt')
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination (Join-Path $packageRoot 'README.md')

$checksums = Get-ChildItem -LiteralPath $packageRoot -Recurse -File |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($packageRoot.Length + 1).Replace('\', '/')
        $hash = Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256
        "$($hash.Hash)  $relative"
    }
$checksums | Set-Content -LiteralPath (Join-Path $packageRoot 'checksums_sha256.txt') -Encoding ASCII

$zip = Join-Path $distRoot "$packageName.zip"
if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path (Join-Path $packageRoot '*') -DestinationPath $zip -Force
if (!(Test-Path -LiteralPath $zip)) {
    throw "Expected package zip was not created: $zip"
}

Write-Host "Package folder: $packageRoot"
Write-Host "Package zip: $zip"
