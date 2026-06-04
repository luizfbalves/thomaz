/*
    thomaz — system (sysmodules) activity.
    Lists installed sysmodules under /atmosphere/contents, lets the user toggle
    each one's boot2 flag, and offers a reboot when a change requires it.
*/

#pragma once

#include <memory>

#include <borealis.hpp>
#include "platform/sysmod/sysmod_store.hpp"

namespace thomaz {

class SystemActivity : public brls::Activity
{
  public:
    explicit SystemActivity(std::shared_ptr<ISysmoduleStore> store);

    CONTENT_FROM_XML_RES("activity/system.xml");

    void onContentAvailable() override;

  private:
    void refreshList();
    void onToggle(const core::Sysmodule& mod, bool enabled);
    void showRebootBanner();

    std::shared_ptr<ISysmoduleStore> store;
    bool rebootPending = false;
};

} // namespace thomaz
