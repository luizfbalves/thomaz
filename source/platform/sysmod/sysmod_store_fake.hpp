#pragma once
#include "platform/sysmod/sysmod_store.hpp"
#include <vector>

namespace thomaz {

// In-memory ISysmoduleStore for the desktop build: a few fictional sysmodules
// whose enabled state toggles in memory.
class FakeSysmoduleStore : public ISysmoduleStore {
  public:
    FakeSysmoduleStore();
    std::vector<core::Sysmodule> list() override;
    bool setEnabled(const std::string& program_id, bool enabled) override;
  private:
    std::vector<core::Sysmodule> mods;
};

} // namespace thomaz
