#pragma once
#include <atomic>
#include <memory>

#include <borealis.hpp>

#include "core/themes/themezer_types.hpp"
#include "platform/http_client.hpp"
#include "platform/themes/theme_install.hpp"

namespace thomaz {

// Resolves a theme/pack's detail (download URLs + preview), then downloads it
// to sd:/themes/ on demand. Phase A stops at downloading the .nxtheme files.
class ThemeDetailActivity : public brls::Activity {
  public:
    ThemeDetailActivity(thomaz::core::ThemeEntry entry, IHttpClient* http);
    ~ThemeDetailActivity() override;

    CONTENT_FROM_XML_RES("activity/theme_detail.xml");
    void onContentAvailable() override;

  private:
    void onResolved(const thomaz::core::ThemeDetail& detail, bool ok);
    void startDownload();
    void refreshActionButton();   // sets label + which action the button performs
    void doApply();
    void doRemove();
    void showBaseMissingDialog();
    void showRebootDialog();

    thomaz::core::ThemeEntry  entry;
    thomaz::core::ThemeDetail detail;
    bool                      resolved = false;
    IHttpClient*              http;
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);

    bool downloaded = false;
    bool applied    = false;
    bool busy       = false;      // guards against double-taps during async work
};

} // namespace thomaz
