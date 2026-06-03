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
        case SyncSituation::InSync:     return PushPlan{ cloudRevision, false };
    }
    // Unreachable: all enumerators are handled above. Present only to satisfy
    // -Wreturn-type; -Wswitch now flags any new SyncSituation that's added.
    return PushPlan{ cloudRevision, false };
}

} // namespace thomaz::core
