# HD Shade Bridge

An ESP32 that serves a phone-friendly web page for Hunter Douglas **Platinum** shades on the home network: drag a shade's rail to set its position, run and save presets. A companion to [HD_Remote_Clone](https://github.com/digamesystems/HD_Remote_Clone) (local checkout not assumed; read it through `gh`), and may move into it. Currently a **dry run**: every command goes through the real send queue with the real frames and timings, but frames are printed on serial instead of sent by an nRF24.

This file holds rules and stable facts only. Status, plans and results live in the docs (see **Where things live**); record new findings there, not here.

## Working conventions
- **Shorthand commands** (as in HD_Remote_Clone; letters combine, e.g. "cp", "dcp", run in the order d → c → p):
  - **"d"**: deploy. **First run the RF check** (see **RF protocol**): if anything differs from HD_Remote_Clone, stop, report it, and don't upload. Then close any serial monitor (below), `pio run -t upload` (builds first; a failed build uploads nothing), then reopen the monitor in its own window: `Start-Process pio -ArgumentList 'device','monitor' -WorkingDirectory <repo>`. Then check the board over HTTP (below).
  - **"c"**: commit all current changes with a descriptive message. Run `pio run` first and halt if it fails: never commit a broken build. If the changes touch code, the **doc scan** (below) must have been done for them; do it now if not. If they touch `lib/` or `test/`, run "t1" too and halt if it fails (John, 2026-11-27).
  - **"t1"**: host tests, `pio test -e native` (outside the sandbox). Needs MSYS2's g++ on the PC.
  - **"p"**: push to `origin/main` (**public** repo digamesystems/hd-shade-bridge). No CI yet.
- **Serial port:** `upload_port` / `monitor_port` in `platformio.ini` (COM11). The number changes if the board is plugged into another USB port: if COM11 is missing, look for the "Silicon Labs CP210x" entry (`Get-CimInstance Win32_PnPEntity`) and ask before changing `platformio.ini`. Run every `pio` command (builds too) and any serial access **outside the Bash sandbox**: inside it the port reads as missing.
- **Build folder and Dropbox:** the repo is inside Dropbox. Builds failed at random with `ranlib`/`ar: unable to rename … File exists`, inside and outside the sandbox, and a rerun always got through. `.pio/` has carried Dropbox's ignore marker since 2026-11-26 (`Set-Content -Path .pio -Stream com.dropbox.ignored -Value 1`; deleting `.pio/` removes it), but the error came back on 2026-11-27 with the marker set, so Windows Defender scanning new files is now the likelier cause. Until that's settled, retry the build when it's this error.
- **Closing the serial monitor** (John, 2026-11-26): close a running `pio device monitor` whenever the serial port is needed (upload, reading the console, sending a command) without asking, and reopen it afterwards. Use exactly `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/close-monitor.ps1` from the repo root (PowerShell tool); that command is allowed in `.claude/settings.json`, and the script stops nothing but `pio device monitor` processes.
- **Checking on the board:** after flashing, exercise the change over the JSON API with `curl` (outside the sandbox) at the board's address: `wifi status` on serial prints it (`http://shade-bridge.local/` also works). In dry run this moves nothing, so it's safe. Clean up test data afterwards (e.g. delete test presets) and say what was left changed.
- **Firmware version** (John, 2026-11-26): `FW_VERSION` in `src/version.h`, `X.Y.Z`; bump the last number with each new feature (0.5.0 → 0.5.1 → … → 0.5.10). Shown at boot, in `help`, at the foot of the web page and in `/api/status`.
- **Doc scan after every code change** (John, 2026-11-26): whenever code changes, before calling the change done or committing it, check the docs against it and fix whatever it made stale, in the same change. Read each doc against the code as it now is, not just the lines you remember touching:
  - `README.md`: Goals, Status table, the page description, Hardware, Quick start, serial command table, Web API table (routes, fields, status codes, timing), How it works (file list, queue and radio behaviour), open questions, next steps.
  - `docs/FEATURE_EXPLORATION.md`: each idea's "Built" notes and open items.
  - `CLAUDE.md`: facts, the RF check table, the "Where things live" table.
  - Code comments and help text that describe behaviour (`help` in `src/main.cpp`, file headers).
  In the summary, say what was checked and what was changed, or that nothing was stale.
- **Decisions come only from John.** Record a project decision (docs, this file, commit messages, code comments, memory) only when John states it directly ("decided", "go with X", or an equally clear instruction). Discussion, a question, or agreeing something is reasonable is not a decision; if something looks decided, quote the wording to record and ask. Attribute ideas and decisions with name and date, e.g. "Idea (John, 2026-11-25)"; label Claude's input "Proposed (Claude)" or "Recommended (Claude)". Facts John reports are observations, recorded as reported.
- **Host-testable logic in `lib/bridgecore/`** (as HD_Remote_Clone's `lib/hdcore/`): code with no Arduino calls, with Unity tests under `test/test_native_*`. The merge rule is there; the sun-position code goes there if schedules are built.
- **Test location** (John, 2026-11-27): tests that need a place on Earth use San José International Airport (SJC, about 37.36° N, 121.93° W). Never John's home coordinates: they stay in the board's flash, out of this public repo.
- **Licensing:** `GPL-2.0-or-later` (`LICENSE`), matching HD_Remote_Clone's firmware so code can move between them. Every new source file starts with `// SPDX-License-Identifier: GPL-2.0-or-later` (`#` for scripts).

## Design rules
- **The page mirrors the shades** (John, 2026-11-25): every control shows the position it sends, so the page never offers a command whose result it can't draw. That's why there are no up/down arrows and no All column. Positions shown are the last ones *sent*; the shades don't report back.
- **Nothing is sent from inside a request.** Web and serial commands add jobs to the queue (`src/txqueue.*`) and return; `loop()` steps the queue. Keep the loop non-blocking, with one exception: while a command is on air, `loop()` waits for it (up to about 1.8 s), because HD_Remote_Clone sends each command's phases back to back (see **RF protocol**).
- **Protocol facts come from HD_Remote_Clone** (`docs/RF_PROTOCOL.md`, `docs/FIRMWARE.md`): cite them, keep frame layouts and timings identical, and don't re-derive them here. A finding that changes a protocol fact belongs in that repo's docs.

## RF protocol: same as HD_Remote_Clone
**Rule (John, 2026-11-26):** don't change code that affects RF protocol behaviour in any way that differs from HD_Remote_Clone. The bridge sends exactly what HD_Remote_Clone's firmware sends, the same way; a change to what goes on air starts in HD_Remote_Clone and is then copied here.

**The RF check**, run before every "d": compare against HD_Remote_Clone's current `main` (read it through `gh` or a fresh clone, never from memory), item by item:

| What | Bridge | HD_Remote_Clone |
|---|---|---|
| Air settings: 2 Mbps, address `E7E7E7` (width 3), CRC-16, auto-ack off, no retries, static 5-byte payload, PA level | `src/radio.cpp` | `src/rf_tools.cpp` `prepareWake()`, `txPa` |
| Re-applying those settings before each command | `src/radio.cpp` | `sendGroup()` / `sendPosition()` call `prepareWake()` |
| Channels 33/52/71, rotation restarting at 33 for each phase, packets back to back for the whole phase, and a command's phases back to back | `src/radio.cpp` `sendPhase()`, `src/txqueue.cpp` `poll()` | `rotateBurst()`, `sendGroup()`, `sendPosition()` |
| Frame layouts: `ID ID 20 GG A1/A2` then `21/22`; `ID ID 50 MM PP` then `32 MM PP` | `src/txqueue.cpp` | `lib/hdcore/src/hd_rf.cpp` `groupPayload()`, `positionPayload()` |
| Timings: group 300 + 330 ms; position 1490 + 300 ms; phase order | `src/txqueue.cpp` `GRP_*`, `POS*` | `src/main.cpp` `kGroupHoldMs`, `kGroupTailMs`; `hd_rf.h` `kPosPhase1Ms`, `kPosPhase2Ms` |
| Pins CE 17 / CSN 5 (VSPI), RF24 library version | `src/radio.cpp`, `platformio.ini` | `include/pins.h`, `platformio.ini` |

Report the HD_Remote_Clone commit checked against, and update the "Checked against" line at the top of `src/radio.cpp`.

**Not RF protocol: queue merging** (John, 2026-11-26: keep it on). Merging chooses which commands to send (groups headed for the same position share one frame, a newer target replaces a waiting one); every frame it produces is one HD_Remote_Clone's `pos` sends the same way, e.g. `pos 0A 255`. The remote ID comes from HD_Remote_Clone's own flash namespace (`hd`/`rid`, read-only) and is never printed or served.

## Shades (stable facts)
- Every shade has **one moving rail** (John, 2026-11-25). **Top-down**: fabric from the top of the window down to the rail. **Bottom-up**: fabric from the bottom up to the rail.
- Groups 1 and 3 are top-down, 2 and 4 bottom-up; `GROUP_TYPE` in `src/web.cpp` is the source of truth.
- A position is the **rail's height**, 0 at the bottom to 255 at the top, on both types: bottom-up confirmed by HD_Remote_Clone's test (group 4, 2026-11-18), top-down from this bridge (groups 1 and 3, John, 2026-11-26). The page's drawing is right for both.

## Safety (applies once frames are really sent)
- **Remote IDs are private:** never commit one (the repo is public). Redact as `xx xx` / `yy yy`, as HD_Remote_Clone does.
- Any transmit path keeps HD_Remote_Clone's **5 s cap** on a single send; never Master Reset an original remote.
- The page has no login by default (`web pass` sets one, over plain HTTP). Revisit before the bridge moves real shades.

## Where things live
| Topic | File |
|---|---|
| Status, page, serial commands, Web API, open questions, next steps | `README.md` |
| Ideas being explored, and what's built of them | `docs/FEATURE_EXPLORATION.md` |
| Send queue, presets in flash | `src/txqueue.*` |
| The merge rule (host-tested) | `lib/bridgecore/src/merge.*`, `test/test_native_merge/` |
| HTTP routes, login, shade types | `src/web.*` |
| The page (HTML/CSS/JS in one string) | `src/page.h` |
| Serial console, setup, loop | `src/main.cpp` |
| Wi-Fi, saved settings, mDNS | `src/net.*` |
| RF protocol, frame timings, shade tests | HD_Remote_Clone `docs/RF_PROTOCOL.md`, `docs/FIRMWARE.md` |
