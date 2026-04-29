# Compatibility

This source tree targets Enshrouded build `1004637`.

The client mod and server proxy both rely on reverse-engineered in-memory
offsets. If Enshrouded updates, the binaries can stop working, fail their byte
checks, or crash. Re-verify offsets before using this on a newer build.

## Current Components

- Client mod: Shroudtopia/EML-compatible DLL loaded from `mods/talents_presets`.
- Server proxy: `dbghelp.dll` proxy loaded beside `enshrouded_server.exe`.
- Game build: `1004637`.
- Client mod id: `talents_presets`.
- Client mod version: `0.5.24`.

## Server Behavior

The current server proxy disables rune cost for talent unlearning globally on
the server. That means normal altar unlearning is also free while the server DLL
is installed.
