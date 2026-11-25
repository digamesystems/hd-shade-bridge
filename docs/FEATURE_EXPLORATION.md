# Feature exploration

Ideas being explored for the bridge, before anything is built. Each one says who raised it and when, what it rests on, and what's still unknown. Ideas that prove useful may move into HD_Remote_Clone's [FEATURE_IDEAS.md](https://github.com/digamesystems/HD_Remote_Clone/blob/main/docs/FEATURE_IDEAS.md).

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

**Proposed (Claude), for discussion:**

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

**Done so far (2026-11-25):** presets saved from the page store groups at the same position as one step, so they share a frame. Merging in the queue isn't built yet, and the three seeded presets are kept as HD_Remote_Clone defines them.

**Open:**

- **A new target while a shade is moving.** This is untested (HD_Remote_Clone FEATURE_IDEAS, open item 2). Merging doesn't make it more likely, since a running job is never changed, but repeated drags will cause it.
- **Mask `0F` (all four groups) hasn't been tried on its own.** Sums such as `06` worked. `FF` hasn't been tried for positions.
