# Building

## Requirements

- Windows 10 or Windows 11.
- Visual Studio 2022 or newer with the MSVC C++ toolchain.
- Windows 10/11 SDK.
- PowerShell 5 or newer.
- Shroudtopia/EML installed in the game for runtime loading.

The scripts look for MSBuild through `vswhere.exe`, then fall back to common
Visual Studio installation paths.

## Build Client

```powershell
.\scripts\build-client.ps1 -Configuration Release
```

Output:

```text
build/x64/Release/client/talents_presets.dll
```

## Build Server Proxy

```powershell
.\scripts\build-server.ps1 -Configuration Release
```

Output:

```text
build/x64/Release/server/dbghelp.dll
```

## Build Release Package

```powershell
.\scripts\package-release.ps1 -Configuration Release -GameVersion 1004637
```

Output:

```text
dist/enshrouded-talents-presets-1004637-0.5.24/
dist/enshrouded-talents-presets-1004637-0.5.24.zip
```
