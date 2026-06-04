#include "app/theme_detail_activity.hpp"
#include "app/app_header.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/image.hpp>
#include <optional>
#include <string>

#include "core/themes/themezer_browse.hpp"
#include "platform/themes/theme_download.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {
// POST GraphQL via the app's http client; returns the response body or nullopt.
core::themezer::GraphQlFetcher makeFetcher(IHttpClient* http) {
    return [http](const std::string& body) -> std::optional<std::string> {
        HttpRequest req;
        req.method = HttpMethod::Post;
        req.url    = "https://api.themezer.net/graphql";
        req.headers.push_back({ "Content-Type", "application/json" });
        req.body = body;
        HttpResponse r = http->request(req);
        return r.ok() ? std::optional<std::string>(r.body) : std::nullopt;
    };
}
} // namespace

ThemeDetailActivity::ThemeDetailActivity(core::ThemeEntry entry, IHttpClient* http)
    : entry(std::move(entry)), http(http) {}

ThemeDetailActivity::~ThemeDetailActivity() { *this->alive = false; }

void ThemeDetailActivity::onContentAvailable() {
    install_header_username(this);

    if (auto* name = (brls::Label*)this->getView("detailName")) name->setText(this->entry.name);
    if (auto* author = (brls::Label*)this->getView("detailAuthor"))
        author->setText("themes/by"_i18n + std::string(" ") + this->entry.author);
    if (auto* note = (brls::Label*)this->getView("downloadNote"))
        note->setText("themes/download_ok"_i18n);

    if (!this->entry.preview_url.empty()) {
        std::string url = this->entry.preview_url;
        IHttpClient* client = this->http;
        auto alive = this->alive;
        brls::async([this, client, url, alive]() {
            HttpResponse r = client->get(url);
            if (!r.ok()) return;
            std::string body = r.body;
            brls::sync([this, alive, body]() {
                if (!alive->load()) return;
                if (auto* img = (brls::Image*)this->getView("detailPreview"))
                    img->setImageFromMem((const unsigned char*)body.data(), (int)body.size());
            });
        });
    }

    if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::VISIBLE);

    core::ThemeEntry e = this->entry;
    IHttpClient* client = this->http;
    auto alive = this->alive;
    brls::async([this, e, client, alive]() {
        core::themezer::GraphQlFetcher fetch = makeFetcher(client);
        core::themezer::DetailResult res = (e.kind == core::ThemeKind::Pack)
            ? core::themezer::pack_detail(e.hex_id, fetch)
            : core::themezer::theme_detail(e.hex_id, fetch);
        bool ok = (res.status == core::themezer::DetailStatus::Ok);
        core::ThemeDetail d = res.detail;
        brls::sync([this, alive, d, ok]() {
            if (!alive->load()) return;
            this->onResolved(d, ok);
        });
    });

    if (auto* btn = this->getView("downloadButton")) {
        btn->registerClickAction([this](brls::View*) {
            brls::sync([this]() { this->startDownload(); });
            return true;
        });
        btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn));
    }
}

void ThemeDetailActivity::onResolved(const core::ThemeDetail& d, bool ok) {
    if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::GONE);
    if (!ok) {
        brls::Application::notify("themes/error_network"_i18n);
        return;
    }
    this->detail   = d;
    this->resolved = true;

    if (auto* desc = (brls::Label*)this->getView("detailDesc"))
        desc->setText(d.description);

    if (d.entry.kind == core::ThemeKind::Pack) {
        if (auto* pl = (brls::Label*)this->getView("partsLabel")) {
            pl->setText("themes/pack_parts"_i18n);
            pl->setVisibility(brls::Visibility::VISIBLE);
        }
        if (auto* box = (brls::Box*)this->getView("partsBox")) {
            for (const auto& p : d.parts) {
                auto* row = new brls::Label();
                row->setText(std::string("- ") + (p.name.empty() ? p.target : p.name));
                row->setFontSize(14.0f);
                row->setTextColor(nvgRGB(0xC8, 0xC8, 0xD0));
                box->addView(row);
            }
        }
    }
}

void ThemeDetailActivity::startDownload() {
    if (!this->resolved) return;
    brls::Application::notify("themes/downloading"_i18n);

    core::ThemeDetail d = this->detail;
    auto alive = this->alive;
    brls::async([alive, d]() {
        ThemeDownloadResult r = download_theme(d);
        std::string msg = r.ok ? "themes/download_ok"_i18n
                               : ("themes/download_fail"_i18n + std::string(": ") + r.error);
        bool ok = r.ok;
        brls::sync([alive, msg, ok]() {
            if (!alive->load()) return;
            brls::Application::notify(msg);
            if (ok) brls::Application::popActivity();
        });
    });
}

} // namespace thomaz
