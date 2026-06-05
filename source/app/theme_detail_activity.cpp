#include "app/theme_detail_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"
#include "app/image_viewer_activity.hpp"

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
// `cancelled` (optional): cooperative abort flag forwarded into the HttpRequest
// so the curl transport can abort in-flight transfers when the activity is torn
// down (CONC-03).
core::themezer::GraphQlFetcher makeFetcher(IHttpClient* http,
                                           std::shared_ptr<std::atomic<bool>> cancelled = nullptr) {
    return [http, cancelled](const std::string& body) -> std::optional<std::string> {
        HttpRequest req;
        req.method    = HttpMethod::Post;
        req.url       = "https://api.themezer.net/graphql";
        req.headers.push_back({ "Content-Type", "application/json" });
        req.body      = body;
        req.cancelled = cancelled;
        HttpResponse r = http->request(req);
        return r.ok() ? std::optional<std::string>(r.body) : std::nullopt;
    };
}
} // namespace

ThemeDetailActivity::ThemeDetailActivity(core::ThemeEntry entry, IHttpClient* http)
    : entry(std::move(entry)), http(http) {}

void ThemeDetailActivity::onContentAvailable() {
    install_header_username(this);
    install_tls_warning_banner(this);

    if (auto* name = (brls::Label*)this->getView("detailName")) name->setText(this->entry.name);
    if (auto* author = (brls::Label*)this->getView("detailAuthor"))
        author->setText("themes/by"_i18n + std::string(" ") + this->entry.author);
    if (auto* note = (brls::Label*)this->getView("downloadNote"))
        note->setText("themes/download_ok"_i18n);

    if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::VISIBLE);

    core::ThemeEntry e  = this->entry;
    IHttpClient* client = this->http;
    auto detResult      = std::make_shared<std::pair<core::ThemeDetail, bool>>(); // (d, ok)
    auto cancelled      = this->cancelledFlag();
    this->runAsync(
        [e, client, detResult, cancelled]() {
            core::themezer::GraphQlFetcher fetch = makeFetcher(client, cancelled);
            core::themezer::DetailResult res = (e.kind == core::ThemeKind::Pack)
                ? core::themezer::pack_detail(e.hex_id, fetch)
                : core::themezer::theme_detail(e.hex_id, fetch);
            detResult->second = (res.status == core::themezer::DetailStatus::Ok);
            detResult->first  = res.detail;
        },
        [this, detResult]() {
            this->onResolved(detResult->first, detResult->second);
        });

    if (auto* btn = this->getView("downloadButton")) {
        btn->registerClickAction([this, alive = this->alive](brls::View*) {
            brls::sync([this, alive]() {
                if (!alive->load()) return;
                if (this->busy) return;
                if (this->applied)         this->doRemove();
                else if (this->downloaded) this->doApply();
                else                       this->startDownload();
            });
            return true;
        });
        btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn));
    }
}

// Fetches `url` into `into`, guarded by the activity's alive flag (same pattern
// as the browse grid).
void ThemeDetailActivity::loadThumb(const std::string& url, brls::Image* into) {
    if (url.empty() || !into) return;
    IHttpClient* client = this->http;
    auto alive = this->alive;
    std::string u = url;
    brls::async([client, alive, u, into]() {
        HttpResponse r = client->get(u);
        if (!r.ok()) return;
        std::string body = r.body;
        brls::sync([alive, body, into]() {
            if (!alive->load()) return;
            into->setImageFromMem((const unsigned char*)body.data(), (int)body.size());
        });
    });
}

// Loads the HD image into the hero, caching bytes by url so revisits are instant.
void ThemeDetailActivity::showGalleryImage(const core::GalleryImage& img) {
    auto* hero = (brls::Image*)this->getView("detailPreview");
    if (!hero || img.url.empty()) return;

    auto it = this->heroCache.find(img.url);
    if (it != this->heroCache.end()) {
        hero->setImageFromMem((const unsigned char*)it->second.data(), (int)it->second.size());
        return;
    }
    IHttpClient* client = this->http;
    auto alive = this->alive;
    std::string url = img.url;
    brls::async([this, client, alive, url]() {
        HttpResponse r = client->get(url);
        if (!r.ok()) return;
        std::string body = r.body;
        brls::sync([this, alive, url, body]() {
            if (!alive->load()) return;
            this->heroCache[url] = body;
            if (auto* hero = (brls::Image*)this->getView("detailPreview"))
                hero->setImageFromMem((const unsigned char*)body.data(), (int)body.size());
        });
    });
}

// Builds the thumbnail strip from detail.gallery. Empty gallery => hide the
// strip and fall back to the browse preview in the hero.
void ThemeDetailActivity::buildGallery() {
    auto* strip = this->getView("thumbStrip");
    auto* row   = (brls::Box*)this->getView("thumbStripRow");
    const auto& g = this->detail.gallery;

    if (g.empty()) {
        if (strip) strip->setVisibility(brls::Visibility::GONE);
        if (!this->entry.preview_url.empty())
            this->loadThumb(this->entry.preview_url, (brls::Image*)this->getView("detailPreview"));
        return;
    }

    if (strip) strip->setVisibility(brls::Visibility::VISIBLE);

    // Built once per resolve. The async loadThumb() calls below capture the raw
    // thumb pointers; this is safe because the strip is never rebuilt during the
    // activity's life (clearViews would dangle in-flight fetches otherwise).
    if (row) {
        row->clearViews();
        for (const auto& item : g) {
            core::GalleryImage gi = item;
            auto* thumb = new brls::Image();
            thumb->setWidth(80.0f);
            thumb->setHeight(45.0f);
            thumb->setCornerRadius(6.0f);
            thumb->setMarginRight(8.0f);
            thumb->setFocusable(true);
            this->loadThumb(gi.thumb_url, thumb);

            thumb->getFocusEvent()->subscribe([this, gi](brls::View*) {
                this->showGalleryImage(gi);
            });
            thumb->registerAction("themes/view_fullscreen"_i18n, brls::BUTTON_A,
                [this, gi](brls::View*) {
                    brls::Application::pushActivity(new ImageViewerActivity(gi, this->http));
                    return true;
                });
            thumb->addGestureRecognizer(new brls::TapGestureRecognizer(thumb, [this, gi]() {
                brls::Application::pushActivity(new ImageViewerActivity(gi, this->http));
            }));

            row->addView(thumb);
        }
    }

    this->showGalleryImage(g.front());
}

void ThemeDetailActivity::onResolved(const core::ThemeDetail& d, bool ok) {
    if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::GONE);
    if (!ok) {
        if (auto* err = (brls::Label*)this->getView("detailError")) {
            err->setText("themes/error_network"_i18n);
            err->setVisibility(brls::Visibility::VISIBLE);
        }
        brls::Application::notify("themes/error_network"_i18n);
        return;
    }
    this->detail   = d;
    this->resolved = true;

    this->buildGallery();

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

    // Reveal the populated content in one pass (no blank-text flash).
    if (auto* content = this->getView("detailContent"))
        content->setVisibility(brls::Visibility::VISIBLE);

    this->downloaded = theme_already_downloaded(this->entry);
    this->applied    = is_active_theme(this->entry);
    this->refreshActionButton();

    // If the .nxtheme is already on disk, classify firmware compatibility now.
    if (this->downloaded) this->analyzeCompat();
}

void ThemeDetailActivity::startDownload() {
    if (!this->resolved) return;
    if (this->busy) return;
    this->setButtonBusy(true);
    brls::Application::notify("themes/downloading"_i18n);

    core::ThemeDetail d = this->detail;
    auto results = std::make_shared<std::pair<bool, std::string>>(); // (ok, msg)
    auto cancelled      = this->cancelledFlag();
    this->runAsync(
        [d, results, cancelled]() {
            ThemeDownloadResult r  = download_theme(d, cancelled);
            results->first         = r.ok;
            results->second        = r.ok ? "themes/download_ok"_i18n
                                          : ("themes/download_fail"_i18n + std::string(": ") + r.error);
        },
        [this, results]() {
            this->setButtonBusy(false);
            brls::Application::notify(results->second);
            if (results->first) {
                this->downloaded = true;
                this->refreshActionButton();
                this->analyzeCompat();   // classify now that the .nxtheme is on disk
            }
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

void ThemeDetailActivity::setButtonBusy(bool busy) {
    this->busy = busy;
    auto* btn = this->getView("downloadButton");
    if (!btn) return;
    btn->setFocusable(!busy);
    btn->setAlpha(busy ? 0.5f : 1.0f);
}

// Entry point for the Apply button. Routes risky (layout-incompatible) themes
// through a choice dialog (full vs background-only); safe themes apply directly.
void ThemeDetailActivity::doApply() {
    if (!this->resolved || this->busy) return;
    if (!base_layouts_available(this->detail)) { this->showBaseMissingDialog(); return; }

    if (this->compatChecked && this->compat.overall != CompatRisk::Safe) {
        this->showApplyChoiceDialog();
        return;
    }
    this->doApplyMode(false);
}

void ThemeDetailActivity::doApplyMode(bool background_only) {
    if (!this->resolved || this->busy) return;
    if (!base_layouts_available(this->detail)) { this->showBaseMissingDialog(); return; }

    this->setButtonBusy(true);
    brls::Application::notify(background_only ? "themes/applying_bg_only"_i18n
                                             : "themes/applying"_i18n);

    core::ThemeDetail d = this->detail;
    auto result = std::make_shared<InstallResult>();
    this->runAsync(
        [d, background_only, result]() {
            *result = install_theme(d, background_only);
        },
        [this, background_only, result]() {
            this->setButtonBusy(false);
            if (!result->ok) {
                brls::Application::notify("themes/apply_fail"_i18n + std::string(": ") + result->error);
                return;
            }
            if (!result->warnings.empty()) brls::Application::notify("themes/warn_parts_removed"_i18n);
            brls::Application::notify(background_only ? "themes/apply_bg_ok"_i18n
                                                     : "themes/apply_ok"_i18n);
            this->applied = true;
            this->refreshActionButton();
            this->showRebootDialog();
        });
}

// Classify theme/firmware compatibility from the downloaded .nxtheme files.
// Dry-run only when the base layouts are present. Updates the badge.
void ThemeDetailActivity::analyzeCompat() {
    if (!this->resolved) return;

    if (auto* badge = (brls::Label*)this->getView("compatBadge")) {
        badge->setText("themes/compat_checking"_i18n);
        badge->setTextColor(nvgRGB(0xC8, 0xC8, 0xD0));
        badge->setVisibility(brls::Visibility::VISIBLE);
    }

    core::ThemeDetail d = this->detail;
    bool allow_dry = base_layouts_available(d);
    auto alive = this->alive;
    brls::async([this, d, allow_dry, alive]() {
        FwVersion fw = get_console_firmware();
        ThemeCompat tc = analyze_theme_compat(d, fw, allow_dry);
        brls::sync([this, alive, tc, fw]() {
            if (!alive->load()) return;
            this->compat        = tc;
            this->consoleFw     = fw;
            this->compatChecked = true;
            this->updateCompatBadge();
        });
    });
}

void ThemeDetailActivity::updateCompatBadge() {
    auto* badge = (brls::Label*)this->getView("compatBadge");
    if (!badge) return;
    if (!this->compatChecked) { badge->setVisibility(brls::Visibility::GONE); return; }

    bool allBgOnly = true;
    int  themeFw   = 0;
    for (const auto& p : this->compat.parts) {
        if (p.has_layout) {
            allBgOnly = false;
            if (p.target_firmware > themeFw) themeFw = p.target_firmware;
        }
    }

    std::string text;
    NVGcolor    color = nvgRGB(0xC8, 0xC8, 0xD0);
    switch (this->compat.overall) {
        case CompatRisk::Safe:
            text  = allBgOnly ? "themes/compat_safe"_i18n : "themes/compat_safe_layout"_i18n;
            color = nvgRGB(0x4C, 0xC2, 0x6E);
            break;
        case CompatRisk::Caution:
            text  = "themes/compat_caution"_i18n;
            color = nvgRGB(0xE0, 0xA0, 0x30);
            break;
        case CompatRisk::LikelyBroken:
            text  = "themes/compat_broken"_i18n;
            color = nvgRGB(0xE0, 0x5A, 0x5A);
            break;
    }

    if (this->compat.overall != CompatRisk::Safe && themeFw > 0) {
        text += "\n" + "themes/compat_fw_note"_i18n + " " + fw_int_to_string(themeFw) +
                " · " + "themes/compat_fw_console"_i18n + " " + fw_to_string(this->consoleFw);
    }

    badge->setText(text);
    badge->setTextColor(color);
    badge->setVisibility(brls::Visibility::VISIBLE);
}

// Layout-incompatible theme: let the user pick full apply or the safe
// background-only fallback. Background-only is offered first (recommended).
void ThemeDetailActivity::showApplyChoiceDialog() {
    auto* dialog = new brls::Dialog("themes/apply_choose_title"_i18n);
    dialog->addButton("themes/apply_bg_only"_i18n, [this]() { this->doApplyMode(true); });
    dialog->addButton("themes/apply_full"_i18n,    [this]() { this->doApplyMode(false); });
    dialog->addButton("themes/apply_cancel"_i18n,  []() {});
    dialog->open();
}

void ThemeDetailActivity::doRemove() {
    if (this->busy) return;
    this->setButtonBusy(true);
    brls::Application::notify("themes/removing"_i18n);

    auto result = std::make_shared<InstallResult>();
    this->runAsync(
        [result]() {
            *result = remove_active_theme();
        },
        [this, result]() {
            this->setButtonBusy(false);
            if (!result->ok) {
                brls::Application::notify("themes/remove_fail"_i18n + std::string(": ") + result->error);
                return;
            }
            brls::Application::notify("themes/remove_ok"_i18n);
            this->applied = false;
            this->refreshActionButton();
            this->showRebootDialog();
        });
}

// On-device firmware base-layout extraction (Phase 2 full engine).
// Calls extract_all_base_layouts() — the multi-title driver that opens one
// privileged session and extracts every /lyt/*.szs from qlaunch (…1000),
// Psl (…1007), and MyPage (…1013) into /themes/systemData/ in one pass.
// Requires Application mode (title takeover); the applet gate inside
// extract_all_base_layouts() returns ok=false with the relaunch prompt otherwise.
//
// [Phase 2 verification trigger] — temporary Phase 2 hardware gate.
// printf logs the ExtractAllResult (written_parts count, failed_parts) to the
// hactool log so the human can verify the on-device run without any Phase 3 UI.
// Phase 3 (INTEG-01/05) will add the permanent "Extrair layouts do firmware"
// action and user-facing progress/result messaging; this trigger will be
// promoted or replaced at that point.
void ThemeDetailActivity::doExtract() {
    if (!this->resolved || this->busy) return;

    this->busy = true;
    brls::Application::notify("themes/extracting"_i18n);

    auto alive = this->alive;
    brls::async([this, alive]() {
        // Phase 2 full engine: extract all layouts from all three titles in one run.
        ExtractAllResult res = extract_all_base_layouts();

        // Log the ExtractAllResult summary for hardware verification (Task 2 / Plan 04).
        std::printf("[doExtract] extract_all_base_layouts: ok=%d written=%zu failed=%zu\n",
                    static_cast<int>(res.ok),
                    res.written_parts.size(),
                    res.failed_parts.size());
        if (!res.systemic_error.empty())
            std::printf("[doExtract] systemic_error: %s\n", res.systemic_error.c_str());
        for (const auto& f : res.failed_parts)
            std::printf("[doExtract] failed_part: %s\n", f.c_str());
        for (const auto& w : res.written_parts)
            std::printf("[doExtract] written_part: %s\n", w.c_str());

        brls::sync([this, alive, res]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!res.ok) {
                brls::Application::notify("themes/extract_fail"_i18n + std::string(": ") + res.systemic_error);
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
