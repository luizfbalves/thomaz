#include "app/image_viewer_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/image.hpp>

#include "platform/image_transcode.hpp"

using namespace brls::literals;

namespace thomaz {

ImageViewerActivity::ImageViewerActivity(core::GalleryImage image, IHttpClient* http)
    : image(std::move(image)), http(http) {}

ImageViewerActivity::~ImageViewerActivity() { *this->alive = false; }

void ImageViewerActivity::onContentAvailable() {
    if (auto* caption = (brls::Label*)this->getView("viewerCaption")) {
        if (this->image.label.empty()) {
            caption->setVisibility(brls::Visibility::GONE);  // no phantom gap below the image
        } else {
            caption->setText(this->image.label);
        }
    }

    std::string url = this->image.url;
    if (url.empty()) {
        if (auto* spinner = this->getView("viewerSpinner"))
            spinner->setVisibility(brls::Visibility::GONE);
        return;
    }

    IHttpClient* client = this->http;
    auto alive = this->alive;
    brls::async([this, client, url, alive]() {
        HttpResponse r = client->get(url);
        if (!r.ok()) return;
        // CDN serves WebP; transcode to PNG so stb_image can decode it (worker thread).
        std::string body = thomaz::platform::to_decodable_image(r.body);
        brls::sync([this, alive, body]() {
            if (!alive->load()) return;
            if (auto* spinner = this->getView("viewerSpinner"))
                spinner->setVisibility(brls::Visibility::GONE);
            if (auto* img = (brls::Image*)this->getView("viewerImage")) {
                img->setImageFromMem((const unsigned char*)body.data(), (int)body.size());
                img->setVisibility(brls::Visibility::VISIBLE);
                // Take focus into the viewer so the underlying gallery thumb's blue
                // selection highlight stops bleeding through the dim backdrop, and
                // hide this view's own highlight — a fullscreen image needs no border
                // (B still pops the activity).
                img->setFocusable(true);
                img->setHideHighlight(true);
                // This viewer is a plain Box (no AppletFrame), so B doesn't pop for
                // free once a view here holds focus — register it explicitly.
                img->registerAction("brls/hints/back"_i18n, brls::BUTTON_B,
                    [](brls::View*) { brls::Application::popActivity(); return true; });
                brls::Application::giveFocus(img);
            }
        });
    });
}

} // namespace thomaz
