// Bridge web page and JSON API.
//
//   GET  /              control page
//   GET  /api/status    queue, commanded positions, Wi-Fi, activity log
//   GET  /api/presets   preset names and steps
//   GET  /api/groups    shade type per group (top-down / bottom-up)
//   POST /api/grp       group=1..4|all  dir=up|down
//   POST /api/pos       group=1..4      pos=0..255
//   POST /api/preset    name=<preset>
//   POST /api/clear     drop queued (not yet started) jobs
//
// POST bodies are form-encoded. Commands are queued and the call returns
// at once (202); the page polls /api/status for progress.
#pragma once

namespace web {
void begin();
void poll();
}
