// SPDX-License-Identifier: GPL-2.0-or-later
// Bridge web page and JSON API.
//
//   GET  /              control page
//   GET  /api/status    version, radio, clock and time zone, queue, commanded
//                       positions, Wi-Fi, activity log
//   GET  /api/presets   preset names, steps and schedules
//   GET  /api/groups    shade type per group (top-down / bottom-up)
//   POST /api/grp       group=1..4|all  dir=up|down
//   POST /api/pos       group=1..4      pos=0..255
//   POST /api/preset    name=<preset>
//   POST /api/preset/save      name, groups (bits), replace=1
//   POST /api/preset/delete    name (its schedule goes too)
//   POST /api/preset/schedule  name, time=HH:MM (empty removes), on=1|0
//   POST /api/tz        zone=<IANA name>
//   POST /api/clear     drop queued (not yet started) jobs
//
// POST bodies are form-encoded. Commands are queued and the call returns
// (202) at once, or once a command on air finishes; the page polls
// /api/status for progress. README.md's Web API table has the details.
#pragma once

namespace web {
void begin();
void poll();
}
