/*
    thomaz — settings activity.
    Preferences: UI language; check-for-app-update (GitHub Releases); and refresh
    the cheats database index. Language changes apply on the next launch (Borealis
    loads translations once at startup).
*/

#pragma once

#include <atomic>
#include <memory>

#include <borealis.hpp>
#include "core/update.hpp"
#include "platform/http_client.hpp"

namespace thomaz {

class SettingsActivity : public brls::Activity
{
  public:
    explicit SettingsActivity(IHttpClient* http);
    ~SettingsActivity() override;

    CONTENT_FROM_XML_RES("activity/settings.xml");

    void onContentAvailable() override;

  private:
    void checkForUpdate(brls::Label* status);
    void installUpdate(const core::ReleaseInfo& release, brls::Label* status);
    void refreshDatabase(brls::Label* status);

    IHttpClient* http;
    bool busy = false; // a network action is in flight

    // Reset in the destructor so in-flight async UI callbacks bail.
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
