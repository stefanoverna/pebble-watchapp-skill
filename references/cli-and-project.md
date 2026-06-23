# CLI, SDK Install & Project Structure

Reference for the `pebble` command-line SDK (rePebble SDK 4.x), the project
directory layout, the `package.json` manifest, and resource declarations.

Sources: `developer.repebble.com/sdk/`, `/sdk4/getting-started/`,
`/guides/tools-and-resources/app-metadata/`, `/guides/app-resources/`,
`/guides/tools-and-resources/hardware-information/`, and the
`coredevices/pebble-tool` repo.

---

## SDK & CLI installation

The modern rePebble setup installs a standalone `pebble-tool`; the ARM toolchain
(`arm-none-eabi`) and the QEMU emulator are **not** bundled — they are fetched by
`pebble sdk install`. Requires Python and Node.js.

> ⚠️ **Use Python 3.11–3.13, NOT 3.14.** The emulator's phone simulator
> (`pypkjs`) and the tool both rely on `gevent` / `gevent-websocket`, which break
> on Python 3.14: `pebble logs` produces no output, and `screenshot` / `emu-button`
> / `install` intermittently die with `libpebble2.exceptions.TimeoutError`. Pin a
> known-good interpreter at install time:
> ```bash
> uv tool install --python 3.12 --force pebble-tool
> ```
> Verify with `pebble --version` (the deprecation/SyntaxWarnings it prints are
> harmless). See **Troubleshooting the emulator** below.

**1. System dependencies**

```bash
# macOS
brew install node

# Ubuntu / WSL
sudo apt install nodejs npm libsdl2-2.0-0 libglib2.0-0 libpixman-1-0 zlib1g libsndio7.0

# Fedora
sudo dnf install nodejs SDL2 glib2 pixman zlib
```
Windows: use WSL with Ubuntu and the Ubuntu commands.

**2. Install the CLI** (via the `uv` package manager):

```bash
uv tool install pebble-tool
```

**3. Install the SDK** (downloads toolchain + QEMU):

```bash
pebble sdk install latest        # also: pebble sdk install <version>
pebble sdk list                  # show installed/available SDKs
pebble sdk uninstall 4.4
```

Always verify before assuming: `pebble --version`, `pebble sdk list`.

---

## `pebble` CLI command reference

Confirm exact flags on the installed version with `pebble --help` and
`pebble <command> --help` — the tool evolves.

### Project management
```bash
pebble new-project [--simple] [--javascript] [--worker] [--rocky] NAME
pebble build                     # compile → build/<name>.pbw
pebble clean
pebble convert-project           # migrate an old project to current layout
```
- `--simple` — clean starting point, no sample code
- `--javascript` — include a PebbleKit JS `src/pkjs/` directory
- `--worker` — add a background worker (`src/c/worker.c`)
- `--rocky` — JS (Rocky.js) project instead of C (not this skill's focus)

### Install / run
```bash
pebble install [FILE.pbw]                     # install last build (or a .pbw)
pebble install --emulator aplite|basalt|chalk|diorite|emery   # boot QEMU + install
pebble install --phone <IP>                   # IP shown in the watch's Developer settings
pebble install --logs                         # install then immediately stream logs
pebble install --qemu HOST:PORT
pebble install --serial SERIAL
pebble install --cloudpebble                  # via Dev Connect after `pebble login`
```

### Device / emulator interaction
```bash
pebble logs [--phone <IP>] [--color|--no-color]   # stream APP_LOG output
pebble screenshot [FILENAME] [--no-open]          # capture the current screen
pebble ping
pebble repl
pebble gdb                                         # debug the app in the emulator
pebble kill                                        # stop the running emulator
pebble wipe [--everything]
```

Simulating input & sensors on the emulator (the app must be installed/running):
```bash
pebble emu-button {click|push|release} {back|up|select|down}   # press a button
#   e.g. pebble emu-button click select       # press SELECT once
#   flags: --duration MS, --repeat N, --interval MS
pebble emu-control                  # open the browser control panel (manual poking)
pebble emu-tap [--direction x+|x-|y+|y-|z+|z-]   # simulate an accelerometer tap
pebble emu-accel <file|motion>      # feed accelerometer data
pebble emu-battery [--percent LEVEL] [--charging]
pebble emu-bt-connection [--connected yes|no]    # toggle phone connection
pebble emu-compass [--heading DEG]
pebble emu-set-time / emu-time-format / emu-app-config / emu-set-content-size
```
All `emu-*` and `--emulator` commands accept a platform from
`{aplite,basalt,chalk,diorite,emery,flint,gabbro}`.

**Display model:** the emulator runs **headless** by default — no clickable
window; the screen is read via `pebble screenshot`, buttons via `pebble
emu-button`. Add `--vnc` (e.g. `pebble install --emulator basalt --vnc`) to serve
the QEMU display over VNC + websockify for a live, clickable/keyboard-interactive
watch (keyboard keys map to the Pebble buttons). `emu-control`'s browser panel
covers sensors (accel/compass/connection) only, not buttons.

### Troubleshooting the emulator (`TimeoutError` & friends)

The emulator is a 3-process pipeline:
```
pebble CLI  ──websocket──▶  pypkjs (Python/gevent phone-sim relay)  ──TCP serial──▶  qemu-pebble (firmware)
```
**Every** CLI command (`install`, `screenshot`, `emu-button`, `logs`) opens a
*fresh* websocket to `pypkjs` and, on connect, sends the watch a
`WatchVersionRequest`, waiting for the reply (`fetch_watch_info`). If the reply
doesn't arrive in time you get:
```
libpebble2.exceptions.TimeoutError
```
This is **not** a crash — it means the emulated firmware didn't answer in time.
Diagnosed causes, most common first:

1. **Host CPU starvation (the usual culprit).** `qemu-pebble` emulates ARM in
   software; if the Mac is loaded (antivirus real-time scan e.g. Bitdefender,
   Spotlight `mds_stores` indexing, other VMs), QEMU stalls and the firmware goes
   unresponsive for 10–30 s windows. Any command hitting that window times out,
   then an identical command seconds later succeeds in ~1 s — the hallmark of this
   issue. Check with `ps aux | sort -nrk3 | head` and `uptime` (load average). Fix:
   pause/whitelist the AV, let indexing finish, close other heavy apps. No timeout
   value rescues a firmware frozen longer than the timeout.
2. **Python 3.14.** Makes `pypkjs` (gevent) fragile — it can exit mid-session,
   leaving `qemu-pebble` orphaned with no relay, so every later command times out
   until a fresh `install` respawns it. Reinstall pebble-tool under 3.11–3.13.
3. **Orphaned processes.** QEMU allows only **one** TCP client per serial port; a
   stale `qemu`/`pypkjs` from a prior run blocks new connections. Hard reset:
   ```bash
   pebble kill; pkill -9 -f qemu-pebble; pkill -9 -f pypkjs; pkill -9 -f websockify
   ```
   then re-`install`. The **first** command after a cold boot is always slower
   (firmware boot) — give it a few seconds before screenshotting.

Practical workflow: boot the emulator **once** and reuse it (don't re-`install`
per step); when a command times out, just retry; keep the host quiet. As a last
resort under unavoidable load you can raise libpebble2's default timeouts
(`wait_for_event` in `libpebble2/events/threaded.py`, `send_and_read` in
`libpebble2/communication/__init__.py`, both ~10–15 s) — but this edits
`site-packages` and is silently lost on the next `uv tool upgrade`.

Because `pebble logs` is the least reliable command (it holds a long-lived
websocket), don't depend on it to verify behaviour: prefer **screenshots** plus
**host-compiled unit tests** of any pure C logic (compile the side-effect-free
`.c` with a tiny shim instead of `pebble.h`, run on the Mac).

### Account / SDK / timeline
```bash
pebble login
pebble logout
pebble sdk install latest | install <ver> | uninstall <ver> | list
pebble insert-pin FILE [--id ID]              # timeline pin
pebble delete-pin FILE [--id ID]
pebble data-logging list | download --session-id ID FILENAME
```

### Publishing
```bash
pebble login
pebble publish [--name NAME] [--version VERSION] [--description DESC]
               [--category CATEGORY] [--release-notes TEXT]
               [--icon-small FILE] [--icon-large FILE] [--screenshots FILE ...]
               [--all-platforms]        # also capture static screenshots
               [--no-gif-all-platforms] # GIF capture is ON by default
               [--is-published]         # make the release visible immediately
               [--non-interactive]
```
`pebble publish` uploads to the appstore backend (`appstore-api.repebble.com`)
and, by default, captures rollover GIFs across all supported platforms before
upload. The developer dashboard at `developer.repebble.com/dashboard` is the
web equivalent for managing listings.

---

## Project directory layout

`pebble new-project --simple myapp` produces:

```
myapp/
├── package.json          # project manifest (npm-style + "pebble" key)
├── wscript               # waf build script — rarely edited by hand
├── src/
│   ├── c/
│   │   └── main.c        # main C app entry (#include <pebble.h>)
│   └── pkjs/             # PebbleKit JS (only with --javascript)
│       └── index.js      # runs on the phone: config, web requests, AppMessage
├── resources/            # images, fonts referenced by resources.media
└── build/
    └── myapp.pbw         # compiled output bundle (the installable artifact)
```

- C source lives in `src/c/`; entry file is `src/c/main.c`.
- PebbleKit JS lives in `src/pkjs/index.js`.
- `enableMultiJS: true` allows multiple JS files with CommonJS `require()`.
- Build output is `build/*.pbw`.

---

## `package.json` manifest

The npm-style file carries a `pebble` object with all Pebble-specific config.
Example for a watchapp (interactive, not a face):

```json
{
  "name": "example-app",
  "author": "Your Name",
  "version": "1.0.0",
  "dependencies": {},
  "pebble": {
    "displayName": "Example App",
    "uuid": "5e5b3966-60b3-453a-a83b-591a13ae47d5",
    "sdkVersion": "3",
    "enableMultiJS": true,
    "capabilities": ["location", "configurable", "health"],
    "targetPlatforms": ["aplite", "basalt", "chalk", "diorite", "emery"],
    "watchapp": {
      "watchface": false
    },
    "messageKeys": ["Temperature", "WeatherIcon"],
    "resources": {
      "media": []
    }
  }
}
```

### `pebble` object fields

| Field | Meaning |
|---|---|
| `uuid` | Unique app ID (UUID v4). Generate with `uuidgen`, lowercase. Must be unique to publish. Never hand-edit randomly. |
| `displayName` | Long name shown in appstore / phone / watch. |
| `sdkVersion` | Major SDK version string, e.g. `"3"`. |
| `enableMultiJS` | Boolean; allow multiple JS files with `require()`. |
| `capabilities` | Array of required features: `"location"`, `"configurable"` (has a config page), `"health"`. |
| `targetPlatforms` | Array of platforms to build for. **Omit to build for all.** |
| `watchapp.watchface` | `true` = watchface, `false` = interactive app. Also `hiddenApp`, `onlyShownOnCommunication`. |
| `messageKeys` | AppMessage keys → generate `MESSAGE_KEY_*` (C) and `require('message_keys')` (JS). |
| `resources.media` | Array of resource entries (images, fonts, raw). Max 256. |
| `resources.publishedMedia` | AppGlance / Timeline published resources. |

Note: older Pebble tooling used `appKeys`; current builds use `messageKeys`.
`version` should be `major.minor.0`, and each published release's version must be
greater than every prior published release.

---

## Target platforms

Declare in `pebble.targetPlatforms`. Codenames and hardware:

| Codename | Watch | Resolution | Colors | Shape | App RAM |
|---|---|---|---|---|---|
| `aplite` | Pebble / Pebble Steel | 144 × 168 | 2 (B/W) | rect | 24 KB |
| `basalt` | Pebble Time / Time Steel | 144 × 168 | 64 | rect | 64 KB |
| `chalk` | Pebble Time Round | 180 × 180 | 64 | **round** | 64 KB |
| `diorite` | Pebble 2 | 144 × 168 | 2 (B/W) | rect | 64 KB |
| `flint` | Pebble 2 Duo | 144 × 168 | 2 (B/W) | rect | 64 KB |
| `emery` | Pebble Time 2 (2025) | 200 × 228 | 64 | rect | 128 KB |
| `gabbro` | Pebble Time 2 Round (2025) | 260 × 260 | 64 | **round** | 128 KB |

Note: `flint` is a 144×168 **black-and-white** device (Pebble 2 class), not a
color one — don't assume it matches `emery`. The current `pebble new-project`
default targets all seven. Always read sizes from `PBL_DISPLAY_WIDTH`/
`PBL_DISPLAY_HEIGHT` in C rather than hardcoding them.

See `platforms-and-memory.md` for conditional compilation and memory limits.

---

## Declaring resources (`resources.media`)

Each media entry needs `type`, `name`, `file`. `name` becomes
`RESOURCE_ID_<name>` in C. Files live under `resources/`; `file` is relative to
it. Max 256 resources per app. Optional `targetPlatforms` restricts a resource to
specific builds.

**Bitmap (image):**
```json
{ "type": "bitmap", "name": "WEATHER_HOT_ICON", "file": "hot.png" }
```
Optional image tuning: `"memoryFormat"` (`Smallest`, `1Bit`, `8Bit`,
`1BitPalette`, `2BitPalette`, `4BitPalette`), `"storageFormat"` (`pbi`/`png`),
`"spaceOptimization"` (`"memory"`/`"storage"`). On B/W watches prefer
`1Bit`/`1BitPalette` to save RAM.

**Menu icon** (the app's icon, ~25×25 png):
```json
{ "type": "bitmap", "name": "MENU_ICON", "file": "icon.png", "menuIcon": true }
```

**Platform-specific** (built only into Basalt here):
```json
{ "type": "bitmap", "name": "BACKGROUND", "file": "bg.png", "targetPlatforms": ["basalt"] }
```
Alternatively use tilde-tagged filenames (`bg~color~round.png`) with tags
`color`/`bw`/`rect`/`round` or platform names — all tags must match, most-tags
wins. Prefer descriptive tags (`color`/`round`) over platform names for
forward-compat.

**Font** (`.ttf`; `name` MUST end in the point size):
```json
{ "type": "font", "name": "EXAMPLE_FONT_20", "file": "fonts/example.ttf",
  "characterRegex": "[0-9:]", "compatibility": "2.7" }
```
- `RESOURCE_ID_EXAMPLE_FONT_20` is generated; the trailing number is the size.
- `characterRegex` — optional, embeds only matching glyphs to shrink the font
  (e.g. `"[0-9:]"` for a clock). Big memory win.
- `compatibility: "2.7"` — optional, use pre-SDK-2.8 font rendering.
- Max practical size ~48pt.

**Raw / PDC vector image:** PDC (Pebble Draw Command) vector icons ship as a
`raw` resource, loaded with `gdraw_command_image_create_with_resource()` — see
the PDC section in `references/ui-and-graphics.md` for the SVG→PDC tooling and
drawing pattern.
```json
{ "type": "raw", "name": "QUIET_TIME", "file": "quiet_time.pdc" }
```

Resource pack size cap is platform-dependent (Aplite tightest ~128 kB; color
platforms 256 kB+). Resources live in flash and are only loaded into RAM when you
call `gbitmap_create_with_resource()` / `fonts_load_custom_font()` — load lazily
and destroy promptly.
