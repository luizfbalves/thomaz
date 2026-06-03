#pragma once

#include <borealis.hpp>
#include <atomic>
#include <memory>
#include <vector>

#include "platform/save_service.hpp"
#include "platform/title.hpp"

namespace thomaz {

// Screen 1: lists installed games with their last-backup date; tapping a row
// opens the detail screen for that game.
class SaveManagerActivity : public brls::Activity
{
  public:
    SaveManagerActivity(ITitleService* titleService, ISaveService* saveService);
    ~SaveManagerActivity() override;

    CONTENT_FROM_XML_RES("activity/save_manager.xml");

    void onContentAvailable() override;

  private:
    void populate(const std::vector<InstalledTitle>& titles);

    ITitleService* titleService;
    ISaveService* saveService;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

} // namespace thomaz
