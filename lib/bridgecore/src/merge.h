// SPDX-License-Identifier: GPL-2.0-or-later
// The queue-merging rule (docs/FEATURE_EXPLORATION.md, "Combine commands to
// save air time"), as plain data in and edits out, so it runs and is tested
// on the host. src/txqueue.cpp applies the edits to its own queue.
//
// For a new position target (mask, pos):
//  1. Its groups are taken out of waiting position jobs with another target
//     (positions are absolute: only a group's latest target matters); a job
//     left with no groups is dropped.
//  2. If a waiting position job has the same target, the groups join it.
//  3. Otherwise it's a new job at the end.
// Only waiting jobs after the last waiting non-position job (a group up/down)
// are touched, so a position never moves past one; the job being sent is
// never touched.
#pragma once
#include <stdint.h>

namespace bridge {

// What the rule needs to know about one job in the queue.
struct QueuedJob {
  bool isPos;    // a position command; false = anything else (group up/down)
  uint8_t mask;  // positions: group bits (01 02 04 08, combinable)
  uint8_t pos;   // positions: target, 0..255
};

enum class EditKind : uint8_t {
  Trim,    // job at `index` keeps only the groups in `mask`
  Drop,    // job at `index` is removed; later jobs move up one
  Join,    // job at `index` now has groups `mask` (the new groups added)
  Append,  // a new position job with groups `mask` goes at the end
};

struct Edit {
  EditKind kind;
  int index;     // into the queue as it stands when this edit is applied
  uint8_t mask;  // see EditKind
};

static const int MAX_QUEUE = 32;  // largest queue the planner handles

struct MergePlan {
  bool full;  // no room for the new job: apply nothing, report "queue full"
  int n;      // edits, applied in order
  Edit edits[MAX_QUEUE + 1];
};

// jobs[0..count) is the queue in sending order; jobs[0..first) aren't waiting
// (first = 1 while the head job is being sent, else 0). capacity = most jobs
// the queue can hold (at most MAX_QUEUE).
MergePlan planPosition(const QueuedJob *jobs, int count, int first, int capacity, uint8_t mask,
                       uint8_t pos);

// Applies a plan to a plain array, as txqueue applies it to its queue; returns
// the new count. `pos` is the new target (for an Append).
int applyPlan(QueuedJob *jobs, int count, const MergePlan &plan, uint8_t pos);

}  // namespace bridge
