// SPDX-License-Identifier: GPL-2.0-or-later
#include "merge.h"

namespace bridge {

MergePlan planPosition(const QueuedJob *jobs, int count, int first, int capacity, uint8_t mask,
                       uint8_t pos) {
  MergePlan plan = {};
  if (count > MAX_QUEUE || capacity > MAX_QUEUE) {
    plan.full = true;
    return plan;
  }

  // The jobs the rule may touch: waiting, and after the last non-position job.
  int lo = first;
  for (int k = count - 1; k >= first; k--)
    if (!jobs[k].isPos) {
      lo = k + 1;
      break;
    }

  // Room check before anything changes: a new job is needed unless one joins,
  // and step 1 may free slots.
  int join = -1, emptied = 0;
  for (int k = lo; k < count; k++) {
    if (jobs[k].pos == pos) join = k;
    else if ((jobs[k].mask & mask) && !(jobs[k].mask & ~mask)) emptied++;
  }
  if (join < 0 && count - emptied >= capacity) {
    plan.full = true;
    return plan;
  }

  // Work on a copy of the masks, so indices follow the drops as they happen.
  uint8_t m[MAX_QUEUE], p[MAX_QUEUE];
  int n = count;
  for (int k = 0; k < count; k++) {
    m[k] = jobs[k].mask;
    p[k] = jobs[k].pos;
  }

  // 1. Take the groups out of jobs with another target.
  for (int k = lo; k < n; k++) {
    if (p[k] == pos || !(m[k] & mask)) continue;
    uint8_t left = m[k] & ~mask;
    if (!left) {
      plan.edits[plan.n++] = {EditKind::Drop, k, 0};
      for (int i = k; i + 1 < n; i++) {
        m[i] = m[i + 1];
        p[i] = p[i + 1];
      }
      n--;
      if (join > k) join--;
      k--;
    } else {
      m[k] = left;
      plan.edits[plan.n++] = {EditKind::Trim, k, left};
    }
  }

  // 2. Join a job with the same target, or 3. append.
  if (join >= 0) plan.edits[plan.n++] = {EditKind::Join, join, (uint8_t)(m[join] | mask)};
  else plan.edits[plan.n++] = {EditKind::Append, n, mask};
  return plan;
}

int applyPlan(QueuedJob *jobs, int count, const MergePlan &plan, uint8_t pos) {
  if (plan.full) return count;
  for (int e = 0; e < plan.n; e++) {
    const Edit &ed = plan.edits[e];
    switch (ed.kind) {
      case EditKind::Trim:
      case EditKind::Join:
        jobs[ed.index].mask = ed.mask;
        break;
      case EditKind::Drop:
        for (int i = ed.index; i + 1 < count; i++) jobs[i] = jobs[i + 1];
        count--;
        break;
      case EditKind::Append:
        jobs[count++] = {true, ed.mask, pos};
        break;
    }
  }
  return count;
}

}  // namespace bridge
