# HD Shade Bridge

An experiment: a simple phone-and-tablet web UI for Hunter Douglas **Platinum Technology** shades, served by an ESP32 on the home network. It's a companion to [HD_Remote_Clone](https://github.com/digamesystems/HD_Remote_Clone), the open replacement for the Platinum handheld remote, and may eventually be folded into that project.

> Personal hobby project. Not affiliated with or endorsed by Hunter Douglas. "Hunter Douglas" and "Platinum" are their trademarks and are used here only to say what this works with.

## Goals

- **Echo some of the Platinum App Bridge's everyday features** in a page that's easy to use from a phone or tablet: group up/down, setting a shade's position, and one-tap scenes (presets).
- **Keep it simple.** One small ESP32 firmware, no app to install, and no cloud: the page is served by the device itself.
- **Find out what's worth keeping.** This is largely an experiment. Anything that proves useful is meant to move into HD_Remote_Clone, where the [Web interface and an always-on bridge](https://github.com/digamesystems/HD_Remote_Clone/blob/main/docs/FEATURE_IDEAS.md#web-interface-and-an-always-on-bridge) section of its feature ideas describes the larger plan.

## Status

| Part | State |
|---|---|
| Wi-Fi setup over the serial console, credentials kept in the ESP32's flash | ✅ working |
| Web page and JSON API | ✅ working (tested from a PC; phone use being tried) |
| Send queue with HD_Remote_Clone's frame timings | ✅ working, **dry run** |
| Sending over the nRF24 radio | ⏳ not yet: frames are printed on serial instead |
| Remote ID, pairing, editing presets from the page | ⏳ not yet |

**Dry run** means every command goes through the real queue, with the real frames and timings, but the board prints each frame on the serial console instead of transmitting it. No radio needs to be attached, and no shade moves.

## The page

<img src="docs/screenshot-dark.png" width="560" alt="Shade bridge page in dark theme: four group columns, each with Up and Down buttons and a window graphic whose fabric is dragged to set the position; groups 1 and 3 drawn top-down, groups 2 and 4 bottom-up; an All groups row below">

The page follows the device's light or dark setting.

- **One column per group (1–4).** Each shows the shade as a window with its fabric. Drag the fabric's rail, or tap where you want it, and let go to send that position. The ▲ Up and ▼ Down buttons send group commands, as the handheld's arrows do in green mode.
- **Shade types.** In this house, groups 1 and 3 are top-down shades and groups 2 and 4 are bottom-up, so the fabric is drawn hanging from the top or rising from the bottom, with OPEN and CLOSED marked at the right ends. The types are set in `GROUP_TYPE` in [src/web.cpp](src/web.cpp).
- **All groups:** up and down only. Positions are set per group.
- **Presets:** bedtime, waketime and sunset, with the same steps as HD_Remote_Clone's serial commands.
- **Activity:** what's sending, what's queued, a progress bar, a log, and a button to clear the queue.

Positions shown are the last ones *sent*. The shades only receive, so there's no way to read where they actually are.

## Hardware

Any ESP32 dev board with Wi-Fi works for the dry run. This one is developed on an ESP32-DevKitC-style board (ESP32-D0WDQ6, 4 MB flash, CP2102 USB-serial) using PlatformIO's `esp32dev` profile.

To send for real, it will need an nRF24L01+ module wired as on HD_Remote_Clone's breadboard; see that project's [WIRING.md](https://github.com/digamesystems/HD_Remote_Clone/blob/main/docs/WIRING.md) and [pin map](https://github.com/digamesystems/HD_Remote_Clone/blob/main/docs/FIRMWARE.md#pin-map).

## Quick start

Needs [PlatformIO](https://platformio.org/). `platformio.ini` expects the board on **COM11**; change `upload_port` and `monitor_port` to match yours.

```
pio run -t upload      # build and flash
pio device monitor     # serial console, 115200 baud
```

Then, in the console:

```
wifi scan
wifi ssid <your network>
wifi pass <your password>
wifi connect
```

The board prints its address when it connects. Open it in a browser, or try `http://shade-bridge.local/`. It reconnects on its own after a reboot.

The ESP32 only joins **2.4 GHz** Wi-Fi.

## Serial commands

| Command | What it does |
|---|---|
| `wifi scan` | List nearby networks |
| `wifi ssid <name>` / `wifi pass [password]` | Set the network (blank password for an open network) |
| `wifi connect` | Save the settings and connect |
| `wifi status` | Connection details and the page's address |
| `wifi forget` | Erase the saved network |
| `web pass [password]` | Require a login for the page (user `admin`); blank removes it |
| `grp <1-4\|all> <up\|down>` | Group command |
| `pos <mask> <0-255>` | Position. `mask` is hex group bits, e.g. `05` = groups 1 and 3 |
| `bedtime`, `waketime`, `sunset` | Presets |
| `queue` / `clear` | Show the queue / drop jobs that haven't started |
| `help`, `reboot` | |

The shade commands use the same names as HD_Remote_Clone's firmware, and the web page and serial console share one queue.

## Web API

`POST` bodies are form-encoded. Commands are queued and answered at once with `202`; the page polls `/api/status` for progress.

| Endpoint | Purpose |
|---|---|
| `GET /` | The control page |
| `GET /api/status` | Queue, last-sent positions, Wi-Fi details, activity log |
| `GET /api/groups` | Shade type per group (`top-down` / `bottom-up`) |
| `GET /api/presets` | Preset names and their steps |
| `POST /api/grp` | `group=1..4\|all`, `dir=up\|down` |
| `POST /api/pos` | `group=1..4`, `pos=0..255` |
| `POST /api/preset` | `name=<preset>` |
| `POST /api/clear` | Drop queued jobs; the one sending finishes |

Errors come back as `400` (bad request) or `429` (queue full) with `{"ok":false,"error":"..."}`.

## How it works

```
src/main.cpp      serial console, setup and loop
src/net.*         Wi-Fi client, saved settings, mDNS name
src/txqueue.*     send queue and presets (dry run: prints frames)
src/web.*         HTTP routes, optional login, shade types
src/page.h        the page: HTML, CSS and script in one string
```

Sending a position takes about 1.8 s on air, and a preset several seconds. The web server and the console run in the same loop, so nothing is sent from inside a request: requests add a job to the queue and return, and the queue steps through each job's frames between loop passes. That keeps the page and console responsive while commands play out.

The frame formats and timings come from HD_Remote_Clone's [RF_PROTOCOL.md](https://github.com/digamesystems/HD_Remote_Clone/blob/main/docs/RF_PROTOCOL.md) and [FIRMWARE.md](https://github.com/digamesystems/HD_Remote_Clone/blob/main/docs/FIRMWARE.md):

| Command | Frames (`ID ID MODE GROUP ACTION`) |
|---|---|
| Group up / down | `20 GG A1` / `A2` for 300 ms, then `21` / `22` for 330 ms |
| Position | `50 MM PP` for 1490 ms, then `32 MM PP` for 300 ms |

`GG` is `01/02/04/08` for groups 1–4 or `FF` for All; `MM` is a group mask; `PP` is the position, 0–255.

## Assumptions and open questions

- **Position is the rail's height** on every shade: 0 at the bottom, 255 at the top. The presets agree with this, but it hasn't been checked on a top-down shade. If it's wrong, the drawing for that shade type needs flipping. This ties to open item 1 in HD_Remote_Clone's feature ideas (which rail position frames move).
- **No password by default.** The page is open to anyone on the network until `web pass` is set, and the login travels unencrypted over plain HTTP. That's acceptable for a dry run; it needs rethinking before the bridge moves real shades.
- **Wi-Fi and the shade radio share 2.4 GHz.** The shades use 2433, 2452 and 2471 MHz, which overlap Wi-Fi channels 6 and 11. How well the two coexist on one board is untested.

## Next steps

1. Attach an nRF24 and send for real, reusing HD_Remote_Clone's RF code.
2. Give the bridge its own remote ID and pair it with the groups it controls.
3. Edit presets from the page: set the shades with the drag controls, then save.
4. Decide what moves into HD_Remote_Clone, and how.

## License

Copyright © 2026 John Price.

Licensed under the GNU General Public License, version 2 or (at your option) any later version (`GPL-2.0-or-later`), the same as HD_Remote_Clone's firmware, so code can move between the two projects. See [LICENSE](LICENSE). Source files carry an `SPDX-License-Identifier` line.

Protocol facts (frame formats and timings) come from HD_Remote_Clone's documentation, which credits [spitcool/hd-platinum-rf](https://github.com/spitcool/hd-platinum-rf) for the original RF research.
