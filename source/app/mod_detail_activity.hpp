/*
    thomaz — mod detail activity.
    For one GameBanana mod: resolves its downloadable files, lists them, and on
    tap downloads the file to the staging _incoming folder and imports it into
    this title's staging area.
*/

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include <borealis.hpp>

#include "core/mods/mod_browse.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

class ModDetailActivity : public brls::Activity
{
  public:
    ModDetailActivity(InstalledTitle title, std::uint64_t mod_id, IHttpClient* http);
    ~ModDetailActivity() override;

    CONTENT_FROM_XML_RES("activity/mod_detail.xml");

    void onContentAvailable() override;

  private:
    // Build the file rows on the UI thread from a resolve result.
    void populate(const core::ResolveResult& result);
    // Download then import a single file (off-thread), notifying on completion.
    void startDownload(const core::ModFile& file);

    InstalledTitle title;
    std::uint64_t modId;
    IHttpClient* http;

    // Set false in the destructor so an in-flight async UI callback bails.
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
