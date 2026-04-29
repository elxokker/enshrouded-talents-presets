# Compatibility

This source tree targets Enshrouded Steam build `23008567`.

The client mod relies on reverse-engineered in-memory offsets. If Enshrouded
updates, the client DLL can stop working, fail byte checks, or crash. Re-verify
client offsets before using this on a newer build.

The server proxy searches for the free-unlearn cost check by instruction
signature instead of a fixed RVA, but the signature still needs re-verification
after major server updates.

## Current Components

- Client mod: Shroudtopia/EML-compatible DLL loaded from `mods/talents_presets`.
- Server proxy: `dbghelp.dll` proxy loaded beside `enshrouded_server.exe`.
- Steam build: `23008567`.
- Client mod id: `talents_presets`.
- Client mod version: `0.5.28`.

## Server Behavior

The current server proxy disables rune cost for talent unlearning globally on
the server. That means normal altar unlearning is also free while the server DLL
is installed.
