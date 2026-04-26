param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64')]
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
. (Join-Path $PSScriptRoot 'Find-MSBuild.ps1')

$project = Join-Path $repoRoot 'server\free_unlearn_proxy\free_unlearn_proxy.vcxproj'
$msbuild = Find-MSBuild

& $msbuild $project /m /p:Configuration=$Configuration /p:Platform=$Platform
if ($LASTEXITCODE -ne 0) {
    throw "Server proxy build failed with exit code $LASTEXITCODE."
}

$dll = Join-Path $repoRoot "build\$Platform\$Configuration\server\dbghelp.dll"
if (!(Test-Path -LiteralPath $dll)) {
    throw "Expected output was not created: $dll"
}

Write-Host "Server build output: $dll"
