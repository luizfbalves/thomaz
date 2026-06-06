#include "app/theme_browser_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"
#include "app/theme_detail_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/ime.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/image.hpp>
#include <optional>
#include <string>

#include "core/themes/themezer_browse.hpp"
#include "platform/themes/active_theme_store.hpp"
#include "platform/themes/theme_paths.hpp"
#include "platform/image_transcode.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {
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

ThemeBrowserActivity::ThemeBrowserActivity(IHttpClient* http) : http(http) {}

void ThemeBrowserActivity::onContentAvailable() {
    install_system_status(this);
    install_header_username(this);
    install_tls_warning_banner(this);

    if (auto* tp = this->getView("tabPacks")) {
        tp->registerClickAction([this, alive = this->alive](brls::View*) {
            this->packsMode = true;
            this->updateTabSelection();
            brls::sync([this, alive]() { if (!alive->load()) return; this->reload(); });
            return true;
        });
        tp->addGestureRecognizer(new brls::TapGestureRecognizer(tp));
    }
    if (auto* tt = this->getView("tabThemes")) {
        tt->registerClickAction([this, alive = this->alive](brls::View*) {
            this->packsMode = false;
            this->updateTabSelection();
            brls::sync([this, alive]() { if (!alive->load()) return; this->reload(); });
            return true;
        });
        tt->addGestureRecognizer(new brls::TapGestureRecognizer(tt));
    }
    if (auto* sb = this->getView("searchButton")) {
        sb->registerClickAction([this](brls::View*) { this->openSearch(); return true; });
        sb->addGestureRecognizer(new brls::TapGestureRecognizer(sb));
    }

    this->updateTabSelection();
    this->reload();
}

// Repaint the Packs/Themes badges so the active mode reads as selected (accent)
// and the other as a plain chip. Without this the XML's initial colors never
// change and Packs looks permanently selected.
void ThemeBrowserActivity::updateTabSelection() {
    const NVGcolor sel   = nvgRGB(0x92, 0x77, 0xFF); // thomaz/accent_bright
    const NVGcolor unsel = nvgRGB(0x22, 0x24, 0x2D); // thomaz/surface_2
    if (auto* tp = (brls::Box*)this->getView("tabPacks"))
        tp->setBackgroundColor(this->packsMode ? sel : unsel);
    if (auto* tt = (brls::Box*)this->getView("tabThemes"))
        tt->setBackgroundColor(this->packsMode ? unsel : sel);
}

void ThemeBrowserActivity::reload() {
    this->runQuery(1);
}

void ThemeBrowserActivity::runQuery(int page) {
    // Show the centered loader and hide the list/empty state so stale themes
    // aren't shown over the spinner while a query is in flight.
    if (auto* lb = this->getView("loadingBox")) lb->setVisibility(brls::Visibility::VISIBLE);
    if (auto* rb = this->getView("resultsBox"))  rb->setVisibility(brls::Visibility::GONE);
    if (auto* el = this->getView("emptyLabel"))  el->setVisibility(brls::Visibility::GONE);

    IHttpClient* client = this->http;
    bool packs = this->packsMode;
    std::string q = this->query;
    std::string t = this->target;

    auto results    = std::make_shared<std::pair<bool, core::BrowsePage>>(); // (ok, pg)
    auto cancelled  = this->cancelledFlag();
    this->runAsync(
        [client, packs, q, t, page, results, cancelled]() {
            core::themezer::GraphQlFetcher fetch = makeFetcher(client, cancelled);
            core::themezer::BrowseResult res = packs
                ? core::themezer::browse_packs(q, page, 30, fetch)
                : core::themezer::browse_themes(q, t, page, 30, fetch);
            results->first  = (res.status == core::themezer::BrowseStatus::Ok);
            results->second = res.page;
        },
        [this, results]() {
            if (auto* lb = this->getView("loadingBox")) lb->setVisibility(brls::Visibility::GONE);
            if (!results->first) {
                // Restore the previous list so a network error doesn't leave a blank screen.
                if (auto* rb = this->getView("resultsBox")) rb->setVisibility(brls::Visibility::VISIBLE);
                brls::Application::notify("themes/error_network"_i18n);
                return;
            }
            this->populate(results->second);
        });
}

void ThemeBrowserActivity::loadThumb(const std::string& url, brls::Image* into) {
    if (url.empty() || !into) return;
    IHttpClient* client = this->http;
    std::string u = url;
    auto body      = std::make_shared<std::string>();
    auto ok        = std::make_shared<bool>(false);
    auto cancelled = this->cancelledFlag();
    auto status = std::make_shared<long>(0);
    // Snapshot the current grid generation; if populate() rebuilds the grid
    // before this fetch returns, `into` points at a freed Image and must not be
    // touched (use-after-free on rapid section switches).
    auto gen     = this->listGen;
    auto genAtDispatch = gen->load();
    this->runAsync(
        [client, u, body, ok, status, cancelled]() {
            HttpRequest req;
            req.url       = u;
            req.cancelled = cancelled;
            HttpResponse r = client->request(req);
            *status = r.status;
            // CDN serves WebP; transcode to PNG so stb_image can decode it (worker thread).
            if (r.ok()) { *body = thomaz::platform::to_decodable_image(r.body); *ok = true; }
        },
        [into, body, ok, status, u, gen, genAtDispatch]() {
            // The grid was rebuilt while this thumb was loading — its card (and
            // `into`) has been destroyed. Drop the result instead of writing to
            // freed memory.
            if (gen->load() != genAtDispatch) return;
            // THEME-IMG diagnostic (theme-preview-blank): logging the list-card
            // thumbnail path tells us whether the blank images are limited to the
            // detail hero or affect the whole module (shared fetch/TLS layer).
            if (!*ok) {
                brls::Logger::error("[THEME-IMG] list thumb FAILED status={} url={}", *status, u);
                return;
            }
            into->setImageFromMem((const unsigned char*)body->data(), (int)body->size());
            if (into->getTexture() == 0)
                brls::Logger::error("[THEME-IMG] list thumb DECODE failed ({} bytes) url={}",
                                    body->size(), u);
        });
}

void ThemeBrowserActivity::populate(const core::BrowsePage& pg) {
    auto* box = (brls::Box*)this->getView("resultsBox");
    auto* empty = (brls::Label*)this->getView("emptyLabel");
    if (!box) return;

    this->page       = pg.page;
    this->isComplete = pg.is_complete;

    // Invalidate any thumbnail fetches still in flight for the previous grid —
    // clearViews() is about to free their Image targets.
    this->listGen->fetch_add(1);

    box->setVisibility(brls::Visibility::VISIBLE); // runQuery hid it during the fetch
    box->clearViews();

    if (pg.entries.empty()) {
        if (empty) { empty->setText("themes/no_results"_i18n); empty->setVisibility(brls::Visibility::VISIBLE); }
        return;
    }
    if (empty) empty->setVisibility(brls::Visibility::GONE);

    brls::Box* rowBox = nullptr;
    brls::View* firstCard = nullptr;
    int col = 0;
    for (const auto& entry : pg.entries) {
        if (col == 0) {
            rowBox = new brls::Box(brls::Axis::ROW);
            rowBox->setMarginBottom(12.0f);
            box->addView(rowBox);
        }
        core::ThemeEntry e = entry;

        auto* card = new brls::Box(brls::Axis::COLUMN);
        card->setWidth(384.0f);     // 3 per row fill the ~1200px content width
        card->setMarginRight(12.0f);
        card->setPadding(8.0f, 8.0f, 10.0f, 8.0f);
        card->setCornerRadius(12.0f);
        card->setFocusable(true);
        card->setHideHighlightBackground(true);
        card->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23));
        if (!firstCard) firstCard = card;

        auto* img = new brls::Image();
        img->setWidth(368.0f);      // fills the card inner width; ~16:9
        img->setHeight(207.0f);
        img->setCornerRadius(8.0f);
        card->addView(img);
        this->loadThumb(e.preview_url, img);

        auto* name = new brls::Label();
        name->setText(e.name);
        name->setFontSize(15.0f);
        name->setLineHeight(1.2f);          // names wrap to 2 lines — give them room
        name->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
        name->setMarginTop(8.0f);
        card->addView(name);

        // Plain text only — the Switch font has no glyph for arrow/check
        // symbols (they render as tofu boxes).
        auto* meta = new brls::Label();
        std::string m = "@" + e.author + "  " + std::to_string(e.downloads) +
                        " " + "themes/downloads"_i18n;
        if (theme_already_downloaded(e)) {
            m += "  [";
            m += "themes/downloaded"_i18n;
            m += "]";
        }
        if (is_active_theme(e)) {
            m += "  [";
            m += "themes/applied"_i18n;
            m += "]";
        }
        meta->setText(m);
        meta->setFontSize(12.0f);
        meta->setMarginTop(4.0f);
        meta->setTextColor(nvgRGB(0x92, 0x77, 0xFF));
        card->addView(meta);

        card->registerClickAction([this, e](brls::View*) {
            brls::Application::pushActivity(new ThemeDetailActivity(e, this->http));
            return true;
        });
        card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
        rowBox->addView(card);

        col = (col + 1) % 3;
    }

    if (!this->isComplete) {
        auto* more = new brls::Box(brls::Axis::ROW);
        more->setHeight(48.0f);
        more->setFocusable(true);
        more->setMarginTop(4.0f);
        more->setCornerRadius(10.0f);
        more->setJustifyContent(brls::JustifyContent::CENTER);
        more->setAlignItems(brls::AlignItems::CENTER);
        more->setHideHighlightBackground(true);
        more->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));
        auto* lbl = new brls::Label();
        lbl->setText("themes/load_more"_i18n);
        lbl->setFontSize(16.0f);
        more->addView(lbl);
        more->registerClickAction([this, alive = this->alive](brls::View*) {
            int next = this->page + 1;
            brls::sync([this, alive, next]() { if (!alive->load()) return; this->runQuery(next); });
            return true;
        });
        more->addGestureRecognizer(new brls::TapGestureRecognizer(more));
        box->addView(more);
    }

    if (firstCard) brls::Application::giveFocus(firstCard);
}

void ThemeBrowserActivity::openSearch() {
    brls::Application::getPlatform()->getImeManager()->openForText(
        [this, alive = this->alive](std::string q) {
            if (!alive->load()) return;
            this->query = q;
            brls::sync([this, alive]() { if (!alive->load()) return; this->reload(); });
        },
        "themes/search"_i18n, "themes/search_hint"_i18n, 64);
}

} // namespace thomaz
