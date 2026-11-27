// SPDX-License-Identifier: GPL-2.0-or-later
// Host tests (`pio test -e native`): the queue-merging rule, lib/bridgecore/merge.*.
#include <unity.h>

#include "merge.h"

using namespace bridge;

static const uint8_t G1 = 0x01, G2 = 0x02, G3 = 0x04, G4 = 0x08;
static const int CAP = 16;  // txqueue's MAX_JOBS

void setUp() {}
void tearDown() {}

static QueuedJob pos(uint8_t mask, uint8_t p) { return {true, mask, p}; }
static QueuedJob grp() { return {false, 0xFF, 0}; }

// Plans (mask, p) against q[0..n), applies it, and returns the new count.
static int queue(QueuedJob *q, int n, int first, uint8_t mask, uint8_t p, MergePlan *out = nullptr) {
  MergePlan plan = planPosition(q, n, first, CAP, mask, p);
  if (out) *out = plan;
  return applyPlan(q, n, plan, p);
}

static void assertJob(const QueuedJob &j, bool isPos, uint8_t mask, uint8_t p) {
  TEST_ASSERT_EQUAL(isPos, j.isPos);
  TEST_ASSERT_EQUAL_HEX8(mask, j.mask);
  if (isPos) TEST_ASSERT_EQUAL_UINT8(p, j.pos);
}

void test_empty_queue_appends() {
  QueuedJob q[CAP];
  MergePlan plan;
  int n = queue(q, 0, 0, G2, 200, &plan);
  TEST_ASSERT_FALSE(plan.full);
  TEST_ASSERT_EQUAL(1, plan.n);
  TEST_ASSERT_EQUAL(EditKind::Append, plan.edits[0].kind);
  TEST_ASSERT_EQUAL(1, n);
  assertJob(q[0], true, G2, 200);
}

// FEATURE_EXPLORATION: same position, different groups -> one frame, mask 0A.
void test_same_position_joins() {
  QueuedJob q[CAP] = {pos(G2, 255)};
  MergePlan plan;
  int n = queue(q, 1, 0, G4, 255, &plan);
  TEST_ASSERT_EQUAL(1, plan.n);
  TEST_ASSERT_EQUAL(EditKind::Join, plan.edits[0].kind);
  TEST_ASSERT_EQUAL(1, n);
  assertJob(q[0], true, 0x0A, 255);
}

// Bedtime (08 255, then 02 255) queued from idle: one job, G2+G4.
void test_bedtime_from_idle_is_one_frame() {
  QueuedJob q[CAP];
  int n = queue(q, 0, 0, G4, 255);
  n = queue(q, n, 0, G2, 255);
  TEST_ASSERT_EQUAL(1, n);
  assertJob(q[0], true, G2 | G4, 255);
}

// Same group, newer target -> only the newer one is kept.
void test_newer_target_replaces_waiting() {
  QueuedJob q[CAP] = {pos(G2, 100)};
  MergePlan plan;
  int n = queue(q, 1, 0, G2, 200, &plan);
  TEST_ASSERT_EQUAL(2, plan.n);
  TEST_ASSERT_EQUAL(EditKind::Drop, plan.edits[0].kind);
  TEST_ASSERT_EQUAL(EditKind::Append, plan.edits[1].kind);
  TEST_ASSERT_EQUAL(1, n);
  assertJob(q[0], true, G2, 200);
}

// Overlap: G1+G2 to 100 waiting, then G2 to 200 -> G1 to 100, and G2 to 200.
void test_overlap_trims() {
  QueuedJob q[CAP] = {pos(G1 | G2, 100)};
  MergePlan plan;
  int n = queue(q, 1, 0, G2, 200, &plan);
  TEST_ASSERT_EQUAL(EditKind::Trim, plan.edits[0].kind);
  TEST_ASSERT_EQUAL_HEX8(G1, plan.edits[0].mask);
  TEST_ASSERT_EQUAL(2, n);
  assertJob(q[0], true, G1, 100);
  assertJob(q[1], true, G2, 200);
}

// Different groups, different positions -> no saving: two jobs.
void test_different_groups_and_positions_stay_apart() {
  QueuedJob q[CAP] = {pos(G1, 100)};
  int n = queue(q, 1, 0, G3, 50);
  TEST_ASSERT_EQUAL(2, n);
  assertJob(q[0], true, G1, 100);
  assertJob(q[1], true, G3, 50);
}

// A drop before the job joined: the join's index follows the drop.
void test_join_index_after_drop() {
  QueuedJob q[CAP] = {pos(G2, 100), pos(G3, 200)};
  MergePlan plan;
  int n = queue(q, 2, 0, G2, 200, &plan);
  TEST_ASSERT_EQUAL(2, plan.n);
  TEST_ASSERT_EQUAL(EditKind::Drop, plan.edits[0].kind);
  TEST_ASSERT_EQUAL(0, plan.edits[0].index);
  TEST_ASSERT_EQUAL(EditKind::Join, plan.edits[1].kind);
  TEST_ASSERT_EQUAL(0, plan.edits[1].index);
  TEST_ASSERT_EQUAL(1, n);
  assertJob(q[0], true, G2 | G3, 200);
}

// The job being sent (first = 1) is never touched, even for the same group.
void test_job_being_sent_untouched() {
  QueuedJob q[CAP] = {pos(G2, 100)};
  int n = queue(q, 1, 1, G2, 200);
  TEST_ASSERT_EQUAL(2, n);
  assertJob(q[0], true, G2, 100);
  assertJob(q[1], true, G2, 200);
  // Nor joined: same target as the one being sent still queues anew.
  n = queue(q, n, 1, G4, 100);
  TEST_ASSERT_EQUAL(3, n);
  assertJob(q[0], true, G2, 100);
}

// Nothing merges across a waiting group up/down command.
void test_group_command_is_a_barrier() {
  QueuedJob q[CAP] = {pos(G2, 100), grp()};
  int n = queue(q, 2, 0, G2, 100);  // same target as the job before the barrier
  TEST_ASSERT_EQUAL(3, n);
  assertJob(q[0], true, G2, 100);
  assertJob(q[1], false, 0xFF, 0);
  assertJob(q[2], true, G2, 100);
  n = queue(q, n, 0, G2, 200);  // trims only after the barrier
  TEST_ASSERT_EQUAL(3, n);
  assertJob(q[0], true, G2, 100);
  assertJob(q[2], true, G2, 200);
}

static int fillDistinct(QueuedJob *q) {
  for (int i = 0; i < CAP; i++) q[i] = pos((uint8_t)(1 << (i % 4)), (uint8_t)(i * 10));
  return CAP;
}

void test_full_queue_refuses_a_new_job() {
  QueuedJob q[CAP];
  int n = fillDistinct(q);
  MergePlan plan = planPosition(q, n, 0, CAP, G1, 255);  // no waiting job has 255
  // G1 appears in jobs 0, 4, 8, 12, each alone: dropping them frees room, so it fits.
  TEST_ASSERT_FALSE(plan.full);
  // Groups that free nothing: make every job two groups, so none empties.
  for (int i = 0; i < CAP; i++) q[i] = pos(G1 | G2, (uint8_t)(i * 10));
  plan = planPosition(q, n, 0, CAP, G3, 255);
  TEST_ASSERT_TRUE(plan.full);
  TEST_ASSERT_EQUAL(0, plan.n);
  TEST_ASSERT_EQUAL(CAP, applyPlan(q, n, plan, 255));  // a full plan changes nothing
}

void test_full_queue_still_takes_a_join() {
  QueuedJob q[CAP];
  int n = fillDistinct(q);  // job 5 is G2 to 50
  MergePlan plan = planPosition(q, n, 0, CAP, G4, 50);
  TEST_ASSERT_FALSE(plan.full);
  n = applyPlan(q, n, plan, 50);
  TEST_ASSERT_EQUAL(CAP - 4, n);  // the four G4-only jobs dropped, G4 joined job "G2 to 50"
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_empty_queue_appends);
  RUN_TEST(test_same_position_joins);
  RUN_TEST(test_bedtime_from_idle_is_one_frame);
  RUN_TEST(test_newer_target_replaces_waiting);
  RUN_TEST(test_overlap_trims);
  RUN_TEST(test_different_groups_and_positions_stay_apart);
  RUN_TEST(test_join_index_after_drop);
  RUN_TEST(test_job_being_sent_untouched);
  RUN_TEST(test_group_command_is_a_barrier);
  RUN_TEST(test_full_queue_refuses_a_new_job);
  RUN_TEST(test_full_queue_still_takes_a_join);
  return UNITY_END();
}
