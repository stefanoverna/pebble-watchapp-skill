# Platforms, Memory & Publishing

Cross-platform builds, conditional compilation, memory limits, best practices,
and appstore publishing. Sources:
`developer.repebble.com/guides/best-practices/building-for-every-pebble/`,
`/conserving-battery-life/`, `/guides/tools-and-resources/hardware-information/`,
`/guides/tools-and-resources/app-metadata/`, and the FAQ.

---

## Platform hardware

App RAM is the total heap budget the build reports for an empty app.

| Platform | Device | Resolution | Colors | Shape | App RAM |
|---|---|---|---|---|---|
| `aplite` | Pebble / Steel | 144 × 168 | 2 (B/W) | rect | 24 KB |
| `basalt` | Pebble Time / Time Steel | 144 × 168 | 64 | rect | 64 KB |
| `chalk` | Pebble Time Round | 180 × 180 | 64 | **round** | 64 KB |
| `diorite` | Pebble 2 | 144 × 168 | 2 (B/W) | rect | 64 KB |
| `flint` | Pebble 2 Duo | 144 × 168 | 2 (B/W) | rect | 64 KB |
| `emery` | Pebble Time 2 (2025) | 200 × 228 | 64 | rect | 128 KB |
| `gabbro` | Pebble Time 2 Round (2025) | 260 × 260 | 64 | **round** | 128 KB |

`aplite` is the tightest target — 24 KB heap, 2-color, smallest resource pack.
If supporting it, that's the budget to design against. Note `flint` is 144×168
**B/W** (Pebble 2 class), not a color/large device. A trivial empty app leaves
roughly 23.5 KB free on aplite, ~64 KB on basalt/chalk/diorite/flint, and ~130 KB
on emery/gabbro. Always read sizes from `PBL_DISPLAY_WIDTH`/`PBL_DISPLAY_HEIGHT`
rather than hardcoding.

---

## Conditional compilation

**Prefer feature defines over platform defines** — they adapt to future hardware.

Platform defines (use sparingly):
`PBL_PLATFORM_APLITE`, `PBL_PLATFORM_BASALT`, `PBL_PLATFORM_CHALK`,
`PBL_PLATFORM_DIORITE`, `PBL_PLATFORM_EMERY`, `PBL_PLATFORM_FLINT`.

Feature defines (preferred):
| Concern | Defines | Inline macro |
|---|---|---|
| Color vs B/W | `PBL_COLOR` / `PBL_BW` | `PBL_IF_COLOR_ELSE(a, b)`, `PBL_IF_BW_ELSE(a, b)` |
| Shape | `PBL_RECT` / `PBL_ROUND` | `PBL_IF_RECT_ELSE(a, b)`, `PBL_IF_ROUND_ELSE(a, b)` |
| Screen size | `PBL_DISPLAY_WIDTH`, `PBL_DISPLAY_HEIGHT` | — |
| Microphone | `PBL_MICROPHONE` | `PBL_IF_MICROPHONE_ELSE(a, b)` |
| Health | `PBL_HEALTH` | `PBL_IF_HEALTH_ELSE(a, b)` |
| Compass | `PBL_COMPASS` | — |
| API existence | `PBL_API_EXISTS(fn)` | — |

```c
#if defined(PBL_COLOR)
  text_layer_set_text_color(s_layer, GColorRed);
#else
  text_layer_set_text_color(s_layer, GColorWhite);
#endif

// single statement form:
text_layer_set_text_color(s_layer, PBL_IF_COLOR_ELSE(GColorJaegerGreen, GColorBlack));

GRect r = GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w, 50);

#if PBL_API_EXISTS(health_service_peek_current_value)
  // safe to call newer API
#endif
```
Always size from `layer_get_bounds()`; never hardcode coordinates. On round
watches, center content and avoid corner-anchored layouts.

---

## Memory limits & introspection

- App heap by platform: aplite 24 KB; basalt/chalk/diorite/flint 64 KB;
  emery/gabbro 128 KB.
- Resources live in flash and load into RAM only when you call
  `gbitmap_create_with_resource()` / `fonts_load_custom_font()`. **Destroy them
  as soon as you're done.**
- Introspect at runtime:
  ```c
  APP_LOG(APP_LOG_LEVEL_INFO, "free: %d used: %d",
          (int)heap_bytes_free(), (int)heap_bytes_used());
  ```
- The **"Free RAM" line in `pebble build` output** is the quickest sanity check.

### The #1 cause of OOM/crashes: leaks
Every `*_create` needs a matching `*_destroy`, and a loaded resource needs its
`gbitmap_destroy` / `fonts_unload_custom_font`. Destroy a layer **before** the
font/bitmap it references. Checklist of pairs:

| Create | Destroy |
|---|---|
| `window_create` | `window_destroy` |
| `layer_create` | `layer_destroy` |
| `text_layer_create` | `text_layer_destroy` |
| `bitmap_layer_create` | `bitmap_layer_destroy` |
| `menu_layer_create` | `menu_layer_destroy` |
| `action_bar_layer_create` | `action_bar_layer_destroy` |
| `scroll_layer_create` | `scroll_layer_destroy` |
| `gbitmap_create_with_resource` | `gbitmap_destroy` |
| `fonts_load_custom_font` | `fonts_unload_custom_font` |
| (system font via `fonts_get_system_font`) | **do not unload** |

---

## Best practices & pitfalls

- **Static buffers, not malloc.** Time/format strings go in
  `static char s_buf[N]` written with `snprintf`/`strftime`. `text_layer_set_text`
  stores the pointer, so it must outlive the call — never a stack buffer.
- **Lazy-load resources;** don't hold every image/font in RAM at once. Use
  `characterRegex` to shrink fonts, and `1Bit`/`1BitPalette` formats on B/W
  watches.
- **Redraw only on change.** Do drawing inside a `LayerUpdateProc`; call
  `layer_mark_dirty()` when data changes — never repaint on a tight timer.
- **Battery (watchfaces especially):** tick at `MINUTE_UNIT`/`HOUR_UNIT`, not
  `SECOND_UNIT`, unless seconds are shown. Batch accelerometer samples, filter
  the compass, keep AppMessage infrequent, minimize backlight/vibration.
- **House style:** module state is `static` with an `s_` prefix; one concern per
  window load/unload pair.

---

## rePebble vs legacy (2025–2026 context)

- `developer.repebble.com` is the new official home (2025 rePebble revival);
  `developer.rebble.io` is the older community mirror with effectively the same
  C API surface — useful as a fallback when a repebble page 404s.
- New 2025 platforms: **Emery** (Pebble Time 2) and **Gabbro** (Pebble Time 2
  Round); also **Flint** (Pebble 2 Duo).
- **Alloy** is a new pure-JavaScript SDK (Moddable-powered) that currently
  targets only Emery/Gabbro — not relevant to C work beyond awareness. The C SDK
  supports the **full lineup** (aplite → gabbro).
- **CloudPebble** is back (browser IDE) alongside the local CLI.

---

## Publishing

1. Build a `.pbw` with a non-beta SDK (`pebble build`).
2. `pebble login`, then `pebble publish`. It
   uploads to `appstore-api.repebble.com` and auto-captures per-platform rollover
   GIFs by default; flags cover name/version/description/category/release notes/
   icons/screenshots (see `cli-and-project.md`). The web dashboard at
   `developer.repebble.com/dashboard` manages listings too.
3. Listing assets: a watchapp and a watchface need different sets (menu icon,
   screenshots, marketing banners). The listing appears in the Pebble mobile
   app's appstore.

Validity rules:
- `pebble.uuid` must be unique and valid (not already in use). Generate with
  `uuidgen`; a UUID mismatch between the watch app and the registered listing is
  a common cause of communication failures.
- At least one `.pbw`, built with a non-beta SDK.
- Each published release's `version` must be **greater than** all prior published
  releases (`major.minor.0`).

Some `/guides/appstore-publishing/*` pages have 404'd on the live rePebble site —
fall back to the `developer.rebble.io` mirror for full asset-dimension detail.
