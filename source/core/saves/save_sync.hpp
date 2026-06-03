#pragma once

namespace thomaz::core {

enum class SyncSituation {
    NoCloud,     // the cloud has no slot for this title
    InSync,      // cloud revision == our last synced revision
    CloudAhead,  // cloud revision > our last synced revision (changed elsewhere)
};

SyncSituation classify(bool cloudExists, int cloudRevision, int syncedRevision);

// What revision to PUT and whether this is a conflict the UI must confirm.
struct PushPlan {
    int  revision;    // revision to send with the PUT (server expects current)
    bool isConflict;  // true => ask the user before overwriting the cloud
};

PushPlan plan_push(SyncSituation situation, int cloudRevision);

} // namespace thomaz::core
