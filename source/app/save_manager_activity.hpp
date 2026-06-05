#pragma once

#include <vector>

#include <borealis.hpp>

#include "app/thomaz_activity.hpp"
#include "platform/save_service.hpp"
#include "platform/title.hpp"
#include "platform/saves/cloud_save_client.hpp"
#include "platform/auth_client.hpp"

namespace thomaz {

// Screen 1: lists installed games with their last-backup date; tapping a row
// opens the detail screen for that game.
class SaveManagerActivity : public ThomazActivity
{
  public:
    SaveManagerActivity(ITitleService* titleService, ISaveService* saveService,
                        ICloudSaveClient* cloudSaves, IAuthClient* feed);

    CONTENT_FROM_XML_RES("activity/save_manager.xml");

    void onContentAvailable() override;

  private:
    void populate(const std::vector<InstalledTitle>& titles);

    ITitleService* titleService;
    ISaveService* saveService;
    ICloudSaveClient* cloudSaves;
    IAuthClient* feed;
};

} // namespace thomaz
