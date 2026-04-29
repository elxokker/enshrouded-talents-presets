# Enshrouded Talents Presets

In-game talent presets for Enshrouded, plus an optional dedicated-server proxy
that makes talent unlearning free.

Supported game build: `1004637`

Client mod id: `talents_presets`

Client mod version: `0.5.24`

## Features

- Five in-game talent presets.
- Save the currently assigned talent tree into a preset.
- Apply a preset from the Skills/Talents screen.
- Clear the current talent tree from the panel.
- Rename presets in-game.
- Movable in-game panel that remembers its last position.
- F12 hover-unlearn helper.
- UI language auto-detection with optional manual override.
- Optional server-side free talent unlearn proxy.

## Runtime Requirements

- Enshrouded build `1004637`.
- Shroudtopia/EML installed on each client that uses the panel.
- The server proxy is needed if you want unlearning to cost no runes on a
  dedicated server.

## Usage

1. Open the Skills/Talents tree in-game.
2. Press `F4` to show or hide the preset panel.
3. Drag the panel header to move it.
4. Click a preset name to rename it.
5. Click `Save current` to store your current talent tree.
6. Click `Apply` to clear your current talents and apply that preset.
7. Click `Clear talents` to remove all assigned talents.
8. Hover a node and press `F12` to unlearn that node.

Keep the Skills/Talents tree open while a preset is applying or clearing.

## Language

The client mod detects language in this order:

1. `talent_presets_lang.txt` override in the mod folder.
2. Steam `appmanifest_1203620.acf` language setting.
3. Windows user language.
4. English fallback.

Supported values are `english`, `spanish`, `french`, `german`, `italian`, and
`brazilian`.

## Build

```powershell
.\scripts\build-client.ps1 -Configuration Release
.\scripts\build-server.ps1 -Configuration Release
.\scripts\package-release.ps1 -Configuration Release -GameVersion 1004637
```

See [docs/BUILDING.md](docs/BUILDING.md) for full build instructions.

## Install

See [docs/INSTALLATION.md](docs/INSTALLATION.md).

## Compatibility

This is a reverse-engineered mod for Enshrouded build `1004637`. Offsets must be
re-verified after game updates. See [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).

## Repository Layout

```text
client/talents_presets/       Client Shroudtopia/EML mod source.
server/free_unlearn_proxy/    Dedicated-server dbghelp.dll proxy source.
include/                      Minimal Shroudtopia SDK headers used by the client.
examples/                     Example preset file.
scripts/                      Build and packaging scripts.
docs/                         Installation, build, and compatibility notes.
```
