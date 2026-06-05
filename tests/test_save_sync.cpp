#include "doctest.h"
#include "core/saves/save_sync.hpp"

using namespace thomaz::core;

TEST_CASE("classify: nothing in the cloud") {
    CHECK(classify(/*cloudExists=*/false, /*cloudRev=*/0, /*syncedRev=*/0) == SyncSituation::NoCloud);
}

TEST_CASE("classify: in sync when revisions match") {
    CHECK(classify(true, 3, 3) == SyncSituation::InSync);
}

TEST_CASE("classify: cloud ahead when its revision is higher than synced") {
    CHECK(classify(true, 5, 3) == SyncSituation::CloudAhead);
}

TEST_CASE("classify: stale-looking lower cloud revision is treated as in sync") {
    // Shouldn't happen, but never falsely flag a conflict.
    CHECK(classify(true, 2, 3) == SyncSituation::InSync);
}

TEST_CASE("plan_push for each situation") {
    PushPlan a = plan_push(SyncSituation::NoCloud, 0);
    CHECK(a.revision == 0);
    CHECK_FALSE(a.isConflict);

    PushPlan b = plan_push(SyncSituation::InSync, 3);
    CHECK(b.revision == 3);
    CHECK_FALSE(b.isConflict);

    PushPlan c = plan_push(SyncSituation::CloudAhead, 5);
    CHECK(c.revision == 5);   // "send mine" overwrites the current cloud revision
    CHECK(c.isConflict);
}

// --- TEST-04a: classify -> plan_push COMPOSITION (the doUpload decision path) ---
// These cover the composed decision that save_detail_activity.cpp's upload flow relies on:
// given a real (cloudExists, cloudRev, syncedRev) triple, assert the final (revision, isConflict).

TEST_CASE("upload decision: cloud advanced since last sync => conflict") {
    // cloudExists=true, cloudRev=5, syncedRev=3 => cloud is ahead
    auto sit  = classify(/*cloudExists=*/true, /*cloudRev=*/5, /*syncedRev=*/3);
    auto plan = plan_push(sit, 5);
    CHECK(sit == SyncSituation::CloudAhead);
    CHECK(plan.isConflict);
    CHECK(plan.revision == 5);
}

TEST_CASE("upload decision: in sync (clean push) => no conflict") {
    // cloudExists=true, cloudRev=3, syncedRev=3 => in sync, safe overwrite
    auto sit  = classify(/*cloudExists=*/true, /*cloudRev=*/3, /*syncedRev=*/3);
    auto plan = plan_push(sit, 3);
    CHECK(sit == SyncSituation::InSync);
    CHECK_FALSE(plan.isConflict);
    CHECK(plan.revision == 3);
}

TEST_CASE("upload decision: no cloud slot yet => first upload, no conflict") {
    // cloudExists=false => new slot, revision 0
    auto sit  = classify(/*cloudExists=*/false, /*cloudRev=*/0, /*syncedRev=*/0);
    auto plan = plan_push(sit, 0);
    CHECK(sit == SyncSituation::NoCloud);
    CHECK_FALSE(plan.isConflict);
    CHECK(plan.revision == 0);
}
