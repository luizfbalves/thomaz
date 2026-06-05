#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include <borealis.hpp>

#include "app/thomaz_activity.hpp"
#include "core/themes/themezer_types.hpp"
#include "platform/http_client.hpp"
#include "platform/themes/theme_install.hpp"
#include "platform/themes/theme_compat.hpp"

namespace thomaz {

// Resolves a theme/pack's detail (download URLs + preview), then downloads it
// to sd:/themes/ on demand. Phase A stops at downloading the .nxtheme files.
class ThemeDetailActivity : public ThomazActivity {
  public:
    ThemeDetailActivity(thomaz::core::ThemeEntry entry, IHttpClient* http);

    CONTENT_FROM_XML_RES("activity/theme_detail.xml");
    void onContentAvailable() override;

  private:
    void onResolved(const thomaz::core::ThemeDetail& detail, bool ok);
    void loadThumb(const std::string& url, brls::Image* into);
    void buildGallery();
    void showGalleryImage(const thomaz::core::GalleryImage& img);
    void startDownload();
    void refreshActionButton();   // sets label + which action the button performs
    void doApply();                       // entry: routes to choice dialog or full apply
    void doApplyMode(bool background_only);
    void doRemove();
    void doExtract();             // on-device firmware base-layout extraction (spike entry point)
    void analyzeCompat();         // post-download: classify theme/firmware compatibility
    void updateCompatBadge();     // reflect `compat` in the badge label
    void showApplyChoiceDialog(); // full vs background-only when risk != Safe
    void showBaseMissingDialog();
    void showRebootDialog();

    thomaz::core::ThemeEntry  entry;
    thomaz::core::ThemeDetail detail;
    bool                      resolved = false;
    IHttpClient*              http;
    std::unordered_map<std::string, std::string> heroCache; // url -> raw image bytes

    bool downloaded = false;
    bool applied    = false;
    bool busy       = false;      // guards against double-taps during async work

    ThemeCompat compat;           // filled by analyzeCompat() once downloaded
    bool        compatChecked = false;
    FwVersion   consoleFw;        // console firmware captured during analysis
};

} // namespace thomaz
