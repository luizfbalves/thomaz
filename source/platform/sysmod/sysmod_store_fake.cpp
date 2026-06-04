#ifndef __SWITCH__

#include "platform/sysmod/sysmod_store_fake.hpp"

namespace thomaz {

FakeSysmoduleStore::FakeSysmoduleStore() {
    mods.push_back({"00FF0000636C6BFF", "sys-clk", true, true, true});
    mods.push_back({"010000000000bd00", "MissionControl", true, false, true});
    mods.push_back({"420000000000000E", "sys-ftpd", true, true, true});
    mods.push_back({"4200000000000000", "4200000000000000", true, false, false});
}

std::vector<core::Sysmodule> FakeSysmoduleStore::list() {
    return mods;
}

bool FakeSysmoduleStore::setEnabled(const std::string& program_id, bool enabled) {
    for (core::Sysmodule& m : mods) {
        if (m.program_id == program_id) {
            m.enabled = enabled;
            return true;
        }
    }
    return false;
}

} // namespace thomaz

#endif // __SWITCH__
