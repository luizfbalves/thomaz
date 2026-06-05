/*
    thomaz — mod manager activity.
    Lists the mods staged for a single game, lets the user toggle which one is
    active (one-active-per-game), uninstall a staged mod, and import a new mod
    archive dropped in the SD _incoming folder.
*/

#pragma once

#include <memory>

#include <borealis.hpp>
#include "app/thomaz_activity.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

class ModManagerActivity : public ThomazActivity
{
  public:
    ModManagerActivity(InstalledTitle title, IHttpClient* http);

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
};

} // namespace thomaz
