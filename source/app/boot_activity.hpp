#pragma once
#include <functional>
#include <borealis.hpp>
#include "app/thomaz_activity.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"
#include "platform/save_service.hpp"
#include "platform/auth_client.hpp"
#include "platform/saves/cloud_save_client.hpp"

namespace thomaz {

class BootActivity : public ThomazActivity {
  public:
    BootActivity(ITitleService* titleService,
                 IHttpClient* http,
                 ISaveService* saveService,
                 IAuthClient* feed,
                 ICloudSaveClient* cloudSaves);

    CONTENT_FROM_XML_RES("activity/boot.xml");
    void onContentAvailable() override;

  private:
    void goHome();

    ITitleService*    titleService;
    IHttpClient*      http;
    ISaveService*     saveService;
    IAuthClient*      feed;
    ICloudSaveClient* cloudSaves;
};

} // namespace thomaz
