---
name: pebble-watchapp
description: Build C SDK watchapps for Pebble smartwatches with the local `pebble` command-line SDK (rePebble SDK 4.x, developer.repebble.com). Use when scaffolding, writing, building, running, or debugging a native C app or watchface for Pebble — covering the pebble CLI workflow, package.json project manifest, the C UI/graphics APIs (Window, Layer, TextLayer, BitmapLayer, MenuLayer, ActionBarLayer, ScrollLayer, ActionMenu), button/click input and window navigation, drawing and fonts, time/TickTimerService, AppMessage and PebbleKit JS phone communication and Clay config pages, persistent storage, timers/wakeups and event services (battery/connection/health), per-platform builds (aplite/basalt/chalk/diorite/emery/gabbro), memory limits, and appstore publishing. Triggers on Pebble app or watchface work, "pebble build", "pebble new-project", watchapp.c / pebble.h editing, or AppMessage/Clay tasks.
---

# Pebble C SDK Watchapp Development

## Overview

This skill supports building native **C watchapps and watchfaces** for Pebble
smartwatches using the **local `pebble` command-line SDK** (the 2025 rePebble
revival, SDK 4.x, documented at `developer.repebble.com`). It covers the full
loop: scaffold a project, write C against the Pebble API, build a `.pbw`, run it
in the QEMU emulator or on a real watch, read logs, and publish.

Pebble C is plain C99 with a single header (`#include <pebble.h>`). Apps run on
a tiny device — as little as **~24 KB of app RAM on the oldest hardware** — so
memory discipline is not optional. The two rules that prevent almost all bugs:

1. **Every `*_create()` has a matching `*_destroy()`** in the corresponding
   unload/deinit handler. Leaks are the #1 cause of crashes and OOM.
2. **Text/strings are not copied** — `text_layer_set_text()` stores the pointer,
   so back it with a `static` buffer that outlives the call. Never point it at a
   stack-local buffer.

## When to use

Use this skill for any task involving native Pebble app development with the
`pebble` CLI: creating a new watchapp/watchface, editing `src/c/*.c`, wiring
buttons/menus/windows, drawing graphics, formatting time, talking to the phone
via AppMessage/PebbleKit JS, adding a Clay configuration page, persisting
settings, building/running/installing, or publishing to the appstore.

For pure-JavaScript apps (Alloy/Rocky.js) this skill does not apply — those use a
different toolchain and only target the newest watches. This skill is C-first.

## Prerequisites — verify the toolchain first

Before building, confirm the SDK is installed. Run `pebble --version` and
`pebble sdk list`. If the CLI is missing, install it (modern rePebble setup):

```bash
# System deps first (macOS shown; see references/cli-and-project.md for Linux)
brew install node
# The CLI ships via the uv package manager:
uv tool install pebble-tool
# Then fetch the toolchain + QEMU emulator (downloads arm-none-eabi + QEMU):
pebble sdk install latest        # accepts the license prompt; takes a few minutes
```

`uv tool install` puts the `pebble` executable in `~/.local/bin` — ensure that's
on `PATH`. The SDK lands under `~/Library/Application Support/Pebble SDK/SDKs/`
(macOS). Do not assume a specific version — check with `pebble sdk list`, and
only install if needed. Full command and flag reference:
`references/cli-and-project.md`.

## Core workflow

1. **Scaffold.** Prefer copying the ready starter in
   `assets/watchapp-template/` (a minimal but correct watchapp: manifest +
   `main.c` skeleton with the load/unload memory pattern already in place).
   Otherwise `pebble new-project --simple <name>` (add `--javascript` for a
   PebbleKit JS `src/pkjs/index.js`, `--worker` for a background worker). Set
   `watchapp.watchface` to `false` for an interactive app, `true` for a face.

2. **Generate a UUID.** Each app needs a unique `pebble.uuid`. Run `uuidgen`
   (lowercase it) and place it in `package.json`. Never ship the template UUID.

3. **Write C** in `src/c/main.c`. Follow the `init()` → `app_event_loop()` →
   `deinit()` structure and the window `.load`/`.unload` handler pattern. Build
   layout from `layer_get_bounds()`, never hardcoded coordinates, and use the
   `PBL_IF_ROUND_ELSE` / `PBL_IF_COLOR_ELSE` macros for cross-platform code.

4. **Build.** `pebble build` → produces `build/<name>.pbw`. Compile errors and
   the **"Free RAM" line** appear in this output — watch the RAM figure.

5. **Run & test.** `pebble install --emulator basalt` boots the bundled QEMU
   emulator and installs+launches the app. See **Testing & the development
   cycle** below for the full inner loop.

6. **Iterate**, then **publish** with `pebble login` + `pebble publish` (uploads
   to the appstore, auto-capturing per-platform screenshots/GIFs). The developer
   dashboard at `developer.repebble.com/dashboard` manages listings too.

## Testing & the development cycle

There is no unit-test framework for Pebble C — testing means **running the app in
the QEMU emulator** (or on a real watch) and observing it. The inner loop:

```bash
pebble build                              # compile → build/<name>.pbw
pebble install --emulator basalt          # boot QEMU, install + launch the app
pebble logs                               # stream APP_LOG output (Ctrl-C to stop)
```

`pebble install --emulator <platform>` is idempotent: the first call boots QEMU
(the very first call may print a transient "Connection refused" while it starts —
just re-run), later calls reuse the running emulator and reinstall in ~1s. So the
edit→see-it loop is: edit C → `pebble build && pebble install --emulator basalt`.

**Drive and observe the running app** (the app must be installed/running):
```bash
pebble emu-button click select            # press a button: back|up|select|down
pebble screenshot out.png --no-open       # capture the current screen to a PNG
pebble install --logs                      # install and immediately stream logs
```
Other simulation: `pebble emu-tap` (accelerometer), `pebble emu-battery
--percent 20`, `pebble emu-bt-connection --connected no` (test disconnect
handling), `pebble emu-set-time`, `pebble emu-control` (browser control panel).
Full list: `references/cli-and-project.md`.

**Reading screenshots is the primary way to verify UI** in a headless/agent
context — take a screenshot, inspect it, press buttons, screenshot again to
confirm state changed. Use `APP_LOG(APP_LOG_LEVEL_DEBUG, "...", ...)` plus
`pebble logs` to trace logic and catch crashes (a crash prints the app's fault
and PC in the log stream). **But `pebble logs` is the flakiest command** (broken
outright on Python 3.14; holds a fragile long-lived websocket) — don't rely on it
alone. For pure, side-effect-free C logic, **host-compile it** with a tiny shim
instead of `pebble.h` and unit-test on your machine. If any `pebble` command dies
with `libpebble2.exceptions.TimeoutError`, it's almost always the **host starving
QEMU of CPU** (antivirus/Spotlight) or a **Python-3.14 install** — see
*Troubleshooting the emulator* in `references/cli-and-project.md`.

**The emulator is headless by default** — `pebble install --emulator` opens *no*
clickable window; the watch screen is a virtual framebuffer read via
`pebble screenshot`, and buttons are pressed with `pebble emu-button`. To get a
live, clickable/keyboard-interactive watch, add **`--vnc`** (e.g.
`pebble install --emulator basalt --vnc`): QEMU then serves its display over VNC
plus a websockify bridge, viewable in a VNC client or browser-based noVNC, where
keyboard keys map to the Pebble buttons. `pebble emu-control` opens a browser
panel for **sensors only** (accelerometer/compass/connection — not buttons). The
CloudPebble web IDE (`cloudpebble.repebble.com`) shows a clickable on-screen
watch if a full GUI is wanted.

**Test across platforms** by installing into each emulator — at minimum a
rectangular color (`basalt`), a round one (`chalk` or `gabbro`), and B/W
(`aplite`) to catch shape/color assumptions. `pebble build` already compiles all
`targetPlatforms`; switch the emulator with `--emulator <platform>`.

**On real hardware:** enable Developer Mode in the Pebble phone app, then
`pebble install --phone <watch-ip>` (IP shown in the app's developer settings).
`pebble logs --phone <watch-ip>` streams from the device.

## Reference material

Load the relevant reference file when working on that area — they hold exact API
signatures, struct field names, and idiomatic snippets distilled from the
official docs. Do not guess signatures; consult these.

- **`references/cli-and-project.md`** — `pebble` CLI command/flag reference, SDK
  install, project directory layout, the `package.json` manifest (every `pebble`
  field), declaring resources (`resources.media`), and the platform/target list.
  Read this for setup, scaffolding, build/run/install, and manifest edits.

- **`references/ui-and-graphics.md`** — app lifecycle skeleton, Window + window
  stack, Layer & custom `LayerUpdateProc` drawing, the `GContext` drawing
  primitives, TextLayer, fonts (system + custom), colors, BitmapLayer/GBitmap,
  PDC vector icons (`GDrawCommandImage` + the SVG→PDC tooling), and time
  formatting with TickTimerService. Read this for anything visual.

- **`references/interaction.md`** — buttons/clicks (`ClickConfigProvider`,
  single/long/multi/repeating), MenuLayer (full callbacks + working example),
  ActionBarLayer, ScrollLayer, ActionMenu, SimpleMenuLayer, StatusBarLayer,
  NumberWindow, multi-window navigation, vibration and backlight. Read this for
  interactive apps (menus, input, navigation between screens).

- **`references/communication-and-data.md`** — AppMessage (open, register
  callbacks, send/receive a `DictionaryIterator`), the `messageKeys` →
  `MESSAGE_KEY_*` mechanism, the PebbleKit JS (`src/pkjs/index.js`) side,
  Clay config pages, web requests, persistent storage, AppTimer, the Wakeup
  API, background workers, ConnectionService/BatteryStateService/HealthService,
  and `APP_LOG` logging. Read this for phone comms, settings, and services.

- **`references/platforms-and-memory.md`** — per-platform screen sizes/colors/
  shape, conditional-compilation defines (`PBL_COLOR`/`PBL_ROUND`/`PBL_PLATFORM_*`
  and the `PBL_IF_*_ELSE` macros), memory limits per platform, `heap_bytes_free`,
  the create/destroy and static-buffer discipline, battery best practices, and
  appstore publishing rules. Read this when targeting multiple watches, chasing
  memory issues, or preparing to publish.

## Critical conventions (apply to all C code)

- **Pair create/destroy.** For each `window_create`, `layer_create`,
  `text_layer_create`, `bitmap_layer_create`, `menu_layer_create`,
  `gbitmap_create_with_resource`, `fonts_load_custom_font`, etc., add the
  matching `*_destroy` / `gbitmap_destroy` / `fonts_unload_custom_font` in the
  window's `.unload` handler (or `deinit`). Destroy a layer **before** the font
  or bitmap it references.
- **Static buffers for text.** Time strings etc. go in `static char s_buf[N]`
  written with `snprintf`/`strftime`. Do not `malloc`; do not use stack buffers
  with `text_layer_set_text`.
- **House style.** Module-level state is `static` with an `s_` prefix
  (`static TextLayer *s_time_layer;`). One concern per window load/unload pair.
- **Redraw on change only.** Custom drawing happens inside a `LayerUpdateProc`;
  call `layer_mark_dirty()` when data changes — never redraw on a tight timer.
- **Layout from bounds.** `GRect bounds = layer_get_bounds(window_get_root_layer(window));`
  then size everything relative to `bounds.size.w/h` so it adapts to every watch.
- **System fonts are not unloaded.** Only `fonts_unload_custom_font` the fonts
  you loaded via `fonts_load_custom_font`.

## A note on verification

The rePebble docs are evolving (2025–2026), and a few pages/commands shift
between SDK versions. When an exact signature, CLI flag, or platform dimension is
load-bearing and uncertain, confirm against the live source: `pebble --help` /
`pebble <cmd> --help` for the CLI, the symbol index at
`developer.repebble.com/docs/`, or the build output itself. The reference files
flag the specific items most worth re-checking.
