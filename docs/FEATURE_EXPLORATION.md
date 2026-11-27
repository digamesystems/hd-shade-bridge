# Feature exploration

Ideas being explored for the bridge, and what's been built of them. Each one says who raised it and when, what it rests on, and what's still unknown. Ideas that prove useful may move into HD_Remote_Clone's [FEATURE_IDEAS.md](https://github.com/digamesystems/HD_Remote_Clone/blob/main/docs/FEATURE_IDEAS.md).

**TL;DR (summary checked 2026-11-27)**

| Idea | Built | Decided (John) | Open |
|---|---|---|---|
| [Combine commands](#combine-commands-to-save-air-time) | Yes: waiting positions to the same target share one frame, and a newer target replaces a waiting one. Host-tested, checked on the board, confirmed live by John | Keep merging on | A new target while a shade is moving; masks `0F` and `FF` untried for positions |
| [First user test](#first-user-test) | A checklist of what to watch | — | Results not yet recorded |
| [Schedules and a sun trigger](#schedules-and-a-sun-trigger) | One daily clock time per preset, with NTP and a time zone set from the page. Checked over the API in dry run; page controls confirmed by John | Every preset can be scheduled; clock time first; "around now" later; tests use SJC | Not built: "around now", the sun-elevation trigger, learning the threshold, a follow-up resend. For John: location setup, how the threshold is learned, what happens at sunset, whether a resend counts under the RF rule |

## Combine commands to save air time

**Idea (John, 2026-11-25):** where several commands can be sent as one, send one, so the queue finishes sooner.

**Why it matters.** Every position command is about 1.8 s on air (`50` frames for 1490 ms, then `32` frames for 300 ms), and the queue sends one at a time. Setting all four groups one by one takes about 7.2 s; as one command it takes 1.8 s.

**What it rests on.** HD_Remote_Clone's shade test of 2026-11-18 ([RF_PROTOCOL.md](https://github.com/digamesystems/HD_Remote_Clone/blob/main/docs/RF_PROTOCOL.md#absolute-position-experimental)):

- A position frame's mask bits combine: `06` addresses groups 2 and 3 together.
- Positions are absolute, and each shade travels to the target on its own. So a later target for a group makes an earlier one that hasn't been sent pointless.

One frame carries one position, so only commands with the same target can share a frame.

**Where commands can combine:**

| Case | Example | Result |
|---|---|---|
| Same position, different groups | G2 to 255, then G4 to 255 | one frame, mask `0A`, 255 |
| Same group, newer position | G2 to 100, then G2 to 200 while the first is still waiting | keep only G2 to 200 |
| Overlap | G1+G2 to 100 waiting, then G2 to 200 | G1 to 100, and G2 to 200 |
| Different groups, different positions | G1 to 100, G3 to 50 | no saving: two frames |

**Proposed (Claude), for discussion** (built 2026-11-25; see below):

- **Merge when a command is queued**, only into jobs that are still waiting. The job being sent is never changed.
- **The rule, for a new target (mask, pos):**
  1. Remove its groups from every waiting job with a different position, and drop any job left with no groups.
  2. If a waiting job has the same position, add the new groups to its mask.
  3. Otherwise queue it as a new job.

  Order doesn't matter: groups move independently, and only each group's latest target counts.
- **Queue presets as separate position steps**, so they merge with the rest of the queue too. Preset definitions can also be stored already merged: `bedtime` (`08 255`, then `02 255`) becomes one frame, `0A 255`, which saves 1.8 s. `waketime` and `sunset` have no two steps at the same position. The same saving applies to HD_Remote_Clone's `bedtime`.
- **Don't merge group commands (`20` frames) with position frames.** They're different frame types.
- **Log what was merged**, e.g. "queued: G2 to 255 (joined G4 to 255)", so the activity log still explains what went out on air.
- **The page's progress bar and time estimate** need no change: they read the queue.

**Built (2026-11-25, fw 0.4.0-dryrun):**

- The queue merges positions as above. The rule is `planPosition()` in [lib/bridgecore/src/merge.cpp](../lib/bridgecore/src/merge.cpp), with host tests for each case in the table and the edge cases ([test/test_native_merge](../test/test_native_merge/test_main.cpp), 2026-11-27); `enqueuePos()` in [src/txqueue.cpp](../src/txqueue.cpp) applies its edits to the queue. Checked on the board the same day, in dry run (`radio off`): with G1 to 100 sending, G2 to 200 and G4 to 200 joined into one job; G2 to 50 trimmed it to G4 to 200 and queued G2 to 50; G4 to 50 then replaced G4 to 200 and joined, leaving one job, G2+G4 to 50.
- **Drags snap to whole percentages** (fw 0.5.2, 2026-11-27). Found on the board: a preset of G2 and G4 both showing 63% held positions 161 and 160, so it went out as two frames. Equal-looking shades now get equal positions, and merge. Confirmed by John (2026-11-27): a re-saved preset of two groups at the same percentage "works as expected".
- One addition to the proposed rule: only waiting positions after the last waiting group (up/down) command are touched, so a position never moves past an up/down for the same shade. The page doesn't send group commands, but the serial `grp` does.
- Presets are queued one position per job, so they merge too. A job keeps the preset's name in its label ("Bedtime: G2+G4 to 255") unless it merges with a command from elsewhere.
- The log says what happened: `queued`, `merged … now …`, `trimmed … now …`, `replaced`. The page's Activity card and the serial `queue` command list the waiting jobs.
- Presets saved from the page already store groups at the same position as one step. The three seeded presets are kept as HD_Remote_Clone defines them; queue merging now gives `Bedtime` its one-frame saving anyway.

**Open:**

- **A new target while a shade is moving.** This is untested (HD_Remote_Clone FEATURE_IDEAS, open item 2). Merging doesn't make it more likely, since a running job is never changed, but repeated drags will cause it.
- **Mask `0F` (all four groups) hasn't been tried on its own.** Sums such as `06` worked. `FF` hasn't been tried for positions.

## First user test

**Raised by John (2026-11-26):** hand the page to a first-time user, a household member, with the radio live (fw 0.5.1), and see where they get stuck. The page has so far been tried only by the person who designed it.

**What to watch (Proposed, Claude):**

- **No explanation first.** Does the tester find, unaided, that you drag the rail (or tap where you want it)? Do they read "61%" as the share of the window the fabric covers? This tests the design rule that the page mirrors the shades.
- **The pause while a command is on air.** The page waits about 1.8 s per command (each command goes out as HD_Remote_Clone sends it, phases back to back). Does it look stuck, and do they tap again? Repeats are harmless (a newer target replaces a waiting one), but a "sending…" cue may be needed.
- **Saving a preset.** Ask them to set a couple of shades and save them as a new preset. Watch the group checkboxes and the name field.
- **Edit and delete.** Is the red ✕ mode obvious, and is Done easy to find?
- **Where they get stuck**, in their own words.

**Results:** not yet recorded. Add what the tester ran into here, and the changes that follow from it.

## Schedules and a sun trigger

**Idea (John, 2026-11-27):** run presets on a schedule. Bedtime and Waketime by clock time; Sunset mode when the sun gets low enough to glare through the west-facing windows. John wants to try the options; **nothing is decided** beyond the items marked as his below.

**What John said (2026-11-27):**

- Sunset mode is for cutting glare through the west-facing windows.
- Bedtime and Waketime are tied to clock time.
- Setup must stay simple for a non-technical user.
- **Learning the trigger** (his idea): when someone sets the shades for glare, the time tells the bridge where the sun is, so the threshold can be learned instead of entered.
- **The user's own command wins** (his current view): if someone moves a shade after a scheduled preset fires, the schedule's follow-up resend leaves that group alone.
- **No replay after a reboot** (his current inclination): a missed event isn't sent late; the bridge waits for the next scheduled change, since people may have used the handheld meanwhile.
- **Decided:** tests use San José International Airport (SJC) as their location, never the house's coordinates. The form-post security fix waits until later.

**Decided (John, 2026-11-27):**

- **Every preset can be scheduled.** A schedule is an option on the preset itself, not a separate list; presets saved from the page get it too.
- **Clock time first:** one time per preset, every day, on or off. More times or days of the week can come later.
- **Later, "around now":** the user runs a preset and indicates that around the current time is when they want the shades to go there, and the bridge records that time. (Claude's note: the same gesture could later record the sun's position instead, for scenes like Sunset; see learning *N* below.)

Claude's notes for the build: store schedules beside the presets, keyed by name, not inside the preset record, whose fixed flash layout would otherwise reset to the defaults and lose saved presets; and put the "does it fire now" logic in `lib/bridgecore` with host tests (midnight, daylight saving, reboots, the resend rule).

**What a schedule needs (Proposed, Claude):**

1. **The time:** the ESP32's NTP client (`configTzTime`) with a POSIX time-zone string such as `PST8PDT,M3.2.0,M11.1.0`, which handles daylight saving. No schedule fires until the time is known after a boot.
2. **The sun's position, computed on the board:** NOAA's solar position formula (about 50 lines, accurate to about a minute) from date, time, latitude and longitude, giving elevation and azimuth. No internet service. It goes in `lib/bridgecore` with host tests at SJC, checked against NOAA's solar calculator for a few dates.
3. **Schedule entries in flash**, stored like the presets: a preset, a trigger (a clock time, or the sun falling below *N*° in the afternoon), days of the week, on/off.
4. **A once-a-minute check.** Each entry fires at most once a day. The sun trigger fires on the afternoon *crossing* below *N* (falling, after solar noon), not on "below *N*", so a reboot in the evening doesn't fire it again.
5. **One resend** about a minute later, to cover missed commands (about 1 in 35 on the handheld), skipping any group the user has moved since. Scheduled sends use the same queue, merging and 5 s cap as everything else. Open: whether a resend counts as RF protocol behaviour under the rule in CLAUDE.md (like merging, it chooses which commands to send, not how).
6. **The page shows each entry's next run**, e.g. "Sunset mode: today 3:52 pm": also the easiest check that the time and location are right.

**Sunset mode: trigger on sun elevation (Proposed, Claude).** A fixed offset such as "1 hour before sunset" drifts across the year. Glare starts when the sun gets low enough to reach under the eaves, at roughly the same elevation all year, so the trigger is "run Sunset when the sun falls below *N*° in the afternoon". An azimuth condition could be added if a window proves partly shaded; not needed to start. What happens at sunset itself, if anything, is John's call. It fires on cloudy days too.

**Learning *N* (Proposed, Claude, from John's idea):** start with *N* entered by hand. Then log the sun's elevation and azimuth each time Sunset is run by hand; after several runs, suggest a threshold a few degrees above them (people notice glare after it starts), and let the user accept it. Keep the readings, not just the number. A "learn from now" button is the same thing done once.

**Location setup, kept simple (Proposed, Claude):**

- **Time zone in one tap:** the phone's browser knows it (`Intl.DateTimeFormat().resolvedOptions().timeZone`, e.g. `America/Los_Angeles`); a small table maps it to a POSIX string.
- **Coordinates: two number fields**, with a hint to copy them from a map. Whole degrees are enough (below). Phone geolocation won't work: browsers allow it only on HTTPS pages, and the bridge serves plain HTTP.
- **Later, optional:** an address box that asks OpenStreetMap's free geocoder from the phone's browser, so the bridge never touches the internet. Google's geocoding API was set aside: it needs a key and billing, and the key would sit on a device built from a public repo.
- Serial fallbacks, like the Wi-Fi setup: e.g. `tz America/Los_Angeles`, `loc 37 -122`.

**How rough can the location be?** A simulation in the review chat (simplified NOAA formula, 30 s steps): true location the SJC area (the tests' reference point; see CLAUDE.md), a 15° threshold learned once on 1 December in the bridge's assumed coordinates, then used all year. Minutes from the true 15° crossing (negative = early):

| Bridge assumes | Learned N | 1 Mar | 1 Jun | 1 Sep |
|---|---|---|---|---|
| Exact | 15.0° | 0 min | 0 min | +0.5 min |
| Whole degrees (37, −122) | 15.3° | −0.5 min | −1.5 min | −0.5 min |
| Longitude off by 3.7° | 12.8° | −3 min | −3.5 min | −3.5 min |
| Latitude off by 3.3° | 17.2° | −5.5 min | −16 min | −10.5 min |
| The time zone's city (Los Angeles) | 15.0° | −8 min | −19.5 min | −14 min |

Whole-degree coordinates stay within about 2 minutes; defaulting to the time zone's city drifts by up to 20 minutes by summer, too much for glare. Rerun this as a host test once the bridge has its own sun code.

**Built (2026-11-27, fw 0.5.4): clock-time schedules.** Every preset can have one daily time, on or off, stored beside the presets keyed by name (saved presets kept). Time from NTP in a time zone set from the page in one tap (US and Canadian zones and UTC). When a schedule fires is `bridge::shouldFire()` in [lib/bridgecore/src/schedule.cpp](../lib/bridgecore/src/schedule.cpp), with host tests: it fires when the clock enters its minute, once a day; nothing fires in the first minute after a boot (John's view: no replay); a time repeated when the clocks go back runs once. **No follow-up resend yet:** whether a resend counts under the RF rule is still John's to decide. Checked on the handheld over the API in dry run (2026-11-27): the clock matched the PC's, a preset fired in its minute, schedules survived a reboot, and nothing fired after a reboot inside a schedule's minute. The page's controls confirmed by John on PC and phone (2026-11-27). Found by John on the page: the time-zone banner stayed up after the zone was set (its CSS `display:flex` overrode `hidden`); fixed in 0.5.5, checked in a headless-browser render with and without a zone.

**Order (Proposed, Claude):** (1) NTP, time zone, clock schedules and "next run" on the page: built in 0.5.4 (above), except that the page shows each preset's daily time, not a "next run"; (2) sun position and the elevation trigger; (3) learning *N*. Each step is useful alone. "Around now" (John's, above) isn't in this list yet.

**Open for John:** the location setup; how *N* is learned; what Sunset mode does at sunset; whether resends count as RF protocol behaviour.

Background: the original Platinum App Bridge timed scenes on the bridge itself, with weekday/weekend times and, from 2014, fixed offsets from sunrise or sunset, which reviewers found didn't fit their terrain.
