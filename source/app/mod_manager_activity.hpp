/*
    thomaz — mod manager activity.
    Lists the mods staged for a single game, lets the user toggle which one is
    active (one-active-per-game), uninstall a staged mod, and import a new mod
    archive dropped in the SD _incoming folder.
*/

#pragma once

#include <atomic>
#include <memory>

#include <borealis.hpp>
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

class ModManagerActivity : public brls::Activity
{
  public:
    ModManagerActivity(InstalledTitle title, IHttpClient* http);
    ~ModManagerActivity() override;

    CONTENT_FROM_XML_RES("activity/mod_manager.xml");

    void onContentAvailable() override;

  private:
    // (Re)build the staged-mod list into modListBox.
    void refreshList();
    // Show the list of importable archives from _incoming as tappable rows.
    void importFlow();
    // Extract one archive into staging, then refresh.
    void doImport(const std::string& archive_path, const std::string& mod_name);

    InstalledTitle title;
    IHttpClient* http;

    // Set false in the destructor so an in-flight UI callback bails.
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
