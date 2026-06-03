#pragma once
#include "platform/title.hpp"

namespace thomaz {

// libnx ns-backed implementation. Construct after the app starts; it manages
// nsInitialize/nsExit via init()/exit().
class NsTitleService : public ITitleService {
  public:
    bool init();   // nsInitialize; returns true on success
    void exit();   // nsExit
    std::vector<InstalledTitle> listInstalled() override;
};

} // namespace thomaz
