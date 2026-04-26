param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64')]
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
. (Join-Path $PSScriptRoot 'Find-MSBuild.ps1')

$project = Join-Path $repoRoot 'client\talents_presets\talents_presets.vcxproj'
$msbuild = Find-MSBuild

& $msbuild $project /m /p:Configuration=$Configuration /p:Platform=$Platform
if ($LASTEXITCODE -ne 0) {
    throw "Client build failed with exit code $LASTEXITCODE."
}

$dll = Join-Path $repoRoot "build\$Platform\$Configuration\client\talents_presets.dll"
if (!(Test-Path -LiteralPath $dll)) {
    throw "Expected output was not created: $dll"
}

Write-Host "Client build output: $dll"
