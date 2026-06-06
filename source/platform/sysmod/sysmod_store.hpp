#pragma once
#include "core/sysmod/sysmod_types.hpp"
#include <string>
#include <vector>

namespace thomaz {

// Scan a contents root (e.g. /atmosphere/contents) into raw entries: one per
// immediate subfolder, recording exefs.nsp presence, flags/boot2.flag presence,
// and the raw toolbox.json contents. Missing root -> empty. Pure POSIX; runs on
// host and Switch.
std::vector<core::RawSysmoduleEntry> sysmod_scan_contents(const std::string& root);

// Create (enabled=true) or remove (enabled=false) <contents_dir>/flags/boot2.flag.
// Creates the flags/ dir as needed. Returns true on success (including the
// already-in-target-state case).
bool sysmod_set_boot2_flag(const std::string& contents_dir, bool enabled);

// Interface consumed by the UI. Real impl scans /atmosphere/contents.
struct ISysmoduleStore {
    virtual ~ISysmoduleStore() = default;
    virtual std::vector<core::Sysmodule> list() = 0;
    // Enable/disable a sysmodule by its program id. Returns success.
    virtual bool setEnabled(const std::string& program_id, bool enabled) = 0;
};

// POSIX implementation over a fixed contents root (defaults to
// core::sysmod_contents_root()).
class SysmoduleStore : public ISysmoduleStore {
  public:
    explicit SysmoduleStore(std::string contents_root);
    SysmoduleStore();
    std::vector<core::Sysmodule> list() override;
    bool setEnabled(const std::string& program_id, bool enabled) override;
  private:
    std::string root;
};

} // namespace thomaz
