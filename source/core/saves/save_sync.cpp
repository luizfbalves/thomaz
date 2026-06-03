#include "core/saves/save_sync.hpp"

namespace thomaz::core {

SyncSituation classify(bool cloudExists, int cloudRevision, int syncedRevision) {
    if (!cloudExists) return SyncSituation::NoCloud;
    if (cloudRevision > syncedRevision) return SyncSituation::CloudAhead;
    return SyncSituation::InSync;
}

PushPlan plan_push(SyncSituation situation, int cloudRevision) {
    switch (situation) {
        case SyncSituation::NoCloud:    return PushPlan{ 0, false };
        case SyncSituation::CloudAhead: return PushPlan{ cloudRevision, true };
        case SyncSituation::InSync:
        default:                        return PushPlan{ cloudRevision, false };
    }
}

} // namespace thomaz::core
