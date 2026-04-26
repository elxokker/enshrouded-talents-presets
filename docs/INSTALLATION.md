# Installation

## Client

1. Close Enshrouded.
2. Make sure Shroudtopia/EML is installed in the Enshrouded client.
3. Copy the release folder `client/mods/talents_presets`.
4. Paste it into the game folder at `Enshrouded/mods/talents_presets`.
5. Start the game.

The final client path should contain:

```text
Enshrouded/mods/talents_presets/talents_presets.dll
Enshrouded/mods/talents_presets/mod.json
```

## Server

1. Stop the server.
2. Back up any existing `dbghelp.dll` in the server root.
3. Copy `server/server_root/dbghelp.dll` from a release package.
4. Paste it beside `enshrouded_server.exe`.
5. Start the server.

The proxy writes `free_talent_unlearn_proxy.log` in the server root when it is
loaded.

## Uninstall

Remove `Enshrouded/mods/talents_presets` from the client.

On the server, stop the server, delete the installed `dbghelp.dll`, restore your
backup if one existed, then start the server again.
