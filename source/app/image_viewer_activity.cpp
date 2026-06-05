#include "app/image_viewer_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/image.hpp>

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
        std::string body = r.body;
        brls::sync([this, alive, body]() {
            if (!alive->load()) return;
            if (auto* spinner = this->getView("viewerSpinner"))
                spinner->setVisibility(brls::Visibility::GONE);
            if (auto* img = (brls::Image*)this->getView("viewerImage")) {
                img->setImageFromMem((const unsigned char*)body.data(), (int)body.size());
                img->setVisibility(brls::Visibility::VISIBLE);
            }
        });
    });
}

} // namespace thomaz
