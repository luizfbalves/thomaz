#pragma once
#include <atomic>
#include <memory>

#include <borealis.hpp>

#include "core/themes/themezer_types.hpp"
#include "platform/http_client.hpp"

namespace thomaz {

// Fullscreen viewer for a single gallery image. Fetches the full-resolution
// `url` async, showing a spinner until it arrives. B closes (Borealis default).
class ImageViewerActivity : public brls::Activity {
  public:
    ImageViewerActivity(thomaz::core::GalleryImage image, IHttpClient* http);
    ~ImageViewerActivity() override;

    CONTENT_FROM_XML_RES("activity/image_viewer.xml");
    void onContentAvailable() override;

  private:
    thomaz::core::GalleryImage image;
    IHttpClient*               http;
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
