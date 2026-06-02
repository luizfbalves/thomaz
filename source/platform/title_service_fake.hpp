/*
    thomaz — fake title service for desktop/PC builds.
    Returns a hard-coded set of sample games so the game list is visible
    without Switch hardware. Only compiled on non-Switch targets.
*/

#pragma once

#ifndef __SWITCH__

#include "platform/title.hpp"

namespace thomaz {

class FakeTitleService : public ITitleService
{
  public:
    std::vector<InstalledTitle> listInstalled() override;
};

} // namespace thomaz

#endif // !__SWITCH__
