# Pebble Watchapp Skill

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Agent Skill](https://img.shields.io/badge/type-agent%20skill-blue.svg)](https://docs.claude.com/en/docs/claude-code/skills)

An [agent skill](https://docs.claude.com/en/docs/claude-code/skills) that teaches
Claude how to build native **C SDK watchapps and watchfaces** for Pebble
smartwatches using the local `pebble` command-line SDK — the 2025 **rePebble**
revival (SDK 4.x, [developer.repebble.com](https://developer.repebble.com)).

It packages the full development loop into a form Claude can use on demand:
scaffold a project, write C against the Pebble API, build a `.pbw`, run it in the
QEMU emulator or on a real watch, read logs, and publish to the appstore.

## What it covers

- **The `pebble` CLI workflow** — SDK install, `new-project`, `build`,
  `install --emulator`, `logs`, `screenshot`, emulator simulation
  (`emu-button`, `emu-tap`, `emu-battery`, …), and `publish`.
- **The C UI & graphics APIs** — `Window`, `Layer`, `TextLayer`, `BitmapLayer`,
  `MenuLayer`, `ActionBarLayer`, `ScrollLayer`, `ActionMenu`, custom
  `LayerUpdateProc` drawing, fonts, colors, and PDC vector icons.
- **Input & navigation** — buttons/clicks, the window stack, multi-window flows.
- **Phone communication & data** — AppMessage, PebbleKit JS, Clay config pages,
  web requests, persistent storage, timers/wakeups, and event services
  (battery / connection / health).
- **Cross-platform & memory discipline** — per-platform builds
  (`aplite`/`basalt`/`chalk`/`diorite`/`emery`/`gabbro`), the `PBL_IF_*_ELSE`
  macros, memory limits, and the create/destroy + static-buffer rules that
  prevent almost all crashes.

## Installing the skill

Clone this repository into your Claude skills directory:

```bash
git clone https://github.com/stefanoverna/pebble-watchapp-skill.git \
  ~/.claude/skills/pebble-watchapp
```

Claude will then load the skill automatically whenever you ask about Pebble app
or watchface work (e.g. "scaffold a Pebble watchface", "`pebble build`",
"wire up an AppMessage handler").

To update later: `git -C ~/.claude/skills/pebble-watchapp pull`.

## Repository contents

| Path | Purpose |
| --- | --- |
| [`SKILL.md`](SKILL.md) | Skill manifest + the core workflow Claude follows. |
| [`references/cli-and-project.md`](references/cli-and-project.md) | `pebble` CLI reference, SDK install, project layout, `package.json` manifest. |
| [`references/ui-and-graphics.md`](references/ui-and-graphics.md) | App lifecycle, windows, layers, drawing, text, fonts, bitmaps, time. |
| [`references/interaction.md`](references/interaction.md) | Buttons, menus, action bars, navigation, vibration, backlight. |
| [`references/communication-and-data.md`](references/communication-and-data.md) | AppMessage, PebbleKit JS, Clay, storage, timers, services, logging. |
| [`references/platforms-and-memory.md`](references/platforms-and-memory.md) | Per-platform specs, conditional compilation, memory limits, publishing. |
| [`assets/watchapp-template/`](assets/watchapp-template/) | A minimal, correct starter watchapp to copy and build on. |

## Prerequisites

The skill drives the local Pebble toolchain. To actually build and run apps you
need the rePebble SDK installed:

```bash
brew install node
uv tool install pebble-tool
pebble sdk install latest
```

See [`references/cli-and-project.md`](references/cli-and-project.md) for full
setup details (including Linux).

## License

[MIT](LICENSE) © Stefano Verna. Pebble and rePebble are trademarks of their
respective owners; this skill is an independent, unofficial resource.
