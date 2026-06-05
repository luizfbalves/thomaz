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
#include "platform/themes/theme_paths.hpp"
#include "platform/themes/active_theme_store.hpp"
#include "platform/themes/theme_install.hpp"
#include "platform/themes/firmware_extract.hpp"
#include "platform/system/reboot.hpp"

#include <vector>

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
            brls::sync([this]() {
                if (this->applied)         this->doRemove();
                else if (this->downloaded) this->doApply();
                else                       this->startDownload();
            });
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
                row->setLineHeight(1.3f);
                row->setMarginBottom(6.0f);
                row->setTextColor(nvgRGB(0xC8, 0xC8, 0xD0));
                box->addView(row);
            }
        }
    }

    this->downloaded = theme_already_downloaded(this->entry);
    this->applied    = is_active_theme(this->entry);
    this->refreshActionButton();
}

void ThemeDetailActivity::startDownload() {
    if (!this->resolved) return;
    brls::Application::notify("themes/downloading"_i18n);

    core::ThemeDetail d = this->detail;
    auto alive = this->alive;
    brls::async([this, alive, d]() {
        ThemeDownloadResult r = download_theme(d);
        std::string msg = r.ok ? "themes/download_ok"_i18n
                               : ("themes/download_fail"_i18n + std::string(": ") + r.error);
        bool ok = r.ok;
        brls::sync([this, alive, msg, ok]() {
            if (!alive->load()) return;
            brls::Application::notify(msg);
            if (ok) {
                this->downloaded = true;
                this->refreshActionButton();
            }
        });
    });
}

void ThemeDetailActivity::refreshActionButton() {
    auto* lbl = (brls::Label*)this->getView("downloadButtonLabel");
    if (!lbl) return;
    if (this->applied) {
        lbl->setText("themes/remove"_i18n);
    } else if (this->downloaded) {
        bool pack = (this->entry.kind == core::ThemeKind::Pack);
        lbl->setText(pack ? "themes/apply_pack"_i18n : "themes/apply"_i18n);
    } else {
        lbl->setText("themes/download"_i18n);
    }
}

void ThemeDetailActivity::doApply() {
    if (!this->resolved || this->busy) return;
    if (!base_layouts_available(this->detail)) { this->showBaseMissingDialog(); return; }

    this->busy = true;
    brls::Application::notify("themes/applying"_i18n);

    core::ThemeDetail d = this->detail;
    auto alive = this->alive;
    brls::async([this, d, alive]() {
        InstallResult r = install_theme(d);
        brls::sync([this, alive, r]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!r.ok) {
                brls::Application::notify("themes/apply_fail"_i18n + std::string(": ") + r.error);
                return;
            }
            if (!r.warnings.empty()) brls::Application::notify("themes/warn_parts_removed"_i18n);
            brls::Application::notify("themes/apply_ok"_i18n);
            this->applied = true;
            this->refreshActionButton();
            this->showRebootDialog();
        });
    });
}

void ThemeDetailActivity::doRemove() {
    if (this->busy) return;
    this->busy = true;
    brls::Application::notify("themes/removing"_i18n);

    auto alive = this->alive;
    brls::async([this, alive]() {
        InstallResult r = remove_active_theme();
        brls::sync([this, alive, r]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!r.ok) {
                brls::Application::notify("themes/remove_fail"_i18n + std::string(": ") + r.error);
                return;
            }
            brls::Application::notify("themes/remove_ok"_i18n);
            this->applied = false;
            this->refreshActionButton();
            this->showRebootDialog();
        });
    });
}

// On-device firmware base-layout extraction (privileged SPL→hactool chain,
// Phase 1 spike). Runs once per missing target, writes to sd:/themes/systemData
// so a subsequent Apply finds the base layouts. Requires Application mode
// (title takeover); extract_base_layout() returns the relaunch prompt otherwise.
void ThemeDetailActivity::doExtract() {
    if (!this->resolved || this->busy) return;

    // The targets this theme/pack needs (mirrors theme_install's detail_targets).
    std::vector<std::string> targets;
    for (const auto& p : this->detail.parts)
        if (!p.target.empty()) targets.push_back(p.target);
    if (targets.empty()) {
        brls::Application::notify("themes/extract_no_targets"_i18n);
        return;
    }

    this->busy = true;
    brls::Application::notify("themes/extracting"_i18n);

    auto alive = this->alive;
    brls::async([this, alive, targets]() {
        ExtractResult last{ true, "" };
        for (const auto& t : targets) {
            last = extract_base_layout(t);
            if (!last.ok) break;   // stop on first failure (e.g. applet mode)
        }
        brls::sync([this, alive, last]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!last.ok) {
                brls::Application::notify("themes/extract_fail"_i18n + std::string(": ") + last.error);
                return;
            }
            brls::Application::notify("themes/extract_ok"_i18n);
            this->refreshActionButton();
        });
    });
}

void ThemeDetailActivity::showBaseMissingDialog() {
    auto* dialog = new brls::Dialog("themes/base_missing_help"_i18n);
    dialog->addButton("themes/extract_now"_i18n, [this]() { this->doExtract(); });
    dialog->addButton("themes/base_missing_close"_i18n, []() {});
    dialog->open();
}

void ThemeDetailActivity::showRebootDialog() {
    auto* dialog = new brls::Dialog("themes/reboot_prompt"_i18n);
    dialog->addButton("themes/reboot_now"_i18n, []() { thomaz::reboot_to_payload(); });
    dialog->addButton("themes/reboot_later"_i18n, []() {});
    dialog->open();
}

} // namespace thomaz
