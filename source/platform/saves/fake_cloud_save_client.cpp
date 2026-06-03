#include "platform/saves/fake_cloud_save_client.hpp"
#include <ctime>

namespace thomaz {

CloudStatus FakeCloudSaveClient::getStatus(const std::string&, std::uint64_t titleId) {
    CloudStatus s;
    s.ok = true;
    auto it = slots.find(titleId);
    if (it != slots.end()) {
        s.exists    = true;
        s.revision  = it->second.revision;
        s.label     = it->second.label;
        s.updatedAt = it->second.updatedAt;
    }
    return s;
}

CloudPull FakeCloudSaveClient::pull(const std::string&, std::uint64_t titleId) {
    CloudPull p;
    p.ok = true;
    auto it = slots.find(titleId);
    if (it != slots.end()) {
        p.exists    = true;
        p.revision  = it->second.revision;
        p.label     = it->second.label;
        p.blob      = it->second.blob;
        p.updatedAt = it->second.updatedAt;
    }
    return p;
}

CloudPush FakeCloudSaveClient::push(const std::string&, std::uint64_t titleId,
                                    const std::vector<std::uint8_t>& blob,
                                    const std::string& label, int revision) {
    CloudPush r;
    auto it = slots.find(titleId);
    // API accepts an absent revision OR 0 for a brand-new slot; the C++ interface
    // always sends an int, so we treat current=0 for an absent slot.
    int current = it == slots.end() ? 0 : it->second.revision;
    if (revision != current) {
        r.conflict = true;
        return r;
    }
    Slot& slot   = slots[titleId];
    slot.revision  = current + 1;
    slot.label     = label;
    slot.blob      = blob;
    slot.updatedAt = (std::int64_t)std::time(nullptr);
    r.ok          = true;
    r.newRevision = slot.revision;
    return r;
}

} // namespace thomaz
