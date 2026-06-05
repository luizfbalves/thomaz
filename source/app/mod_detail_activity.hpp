/*
    thomaz — mod detail activity.
    For one GameBanana mod: resolves its downloadable files, lists them, and on
    tap downloads the file to the staging _incoming folder and imports it into
    this title's staging area.
*/

#pragma once

#include <cstdint>
#include <memory>

#include <borealis.hpp>

#include "app/thomaz_activity.hpp"
#include "core/mods/mod_browse.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

class ModDetailActivity : public ThomazActivity
{
  public:
    // manual_search: true when the user reached this mod through the global
    // free-text search (the game wasn't auto-resolved), so the mod might belong
    // to a different game — we confirm the install target before staging.
    ModDetailActivity(InstalledTitle title, std::uint64_t mod_id, IHttpClient* http,
                      bool manual_search = false);

    CONTENT_FROM_XML_RES("activity/mod_detail.xml");

    void onContentAvailable() override;

  private:
    // Build the file rows on the UI thread from a resolve result.
    void populate(const core::ResolveResult& result);
    // Confirm the target game (manual search only), then startDownload.
    void confirmAndDownload(const core::ModFile& file);
    // Download then import a single file (off-thread), notifying on completion.
    void startDownload(const core::ModFile& file);

    InstalledTitle title;
    std::uint64_t modId;
    IHttpClient* http;
    bool manualSearch;
};

} // namespace thomaz
