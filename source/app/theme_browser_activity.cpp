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

using namespace brls::literals;

namespace thomaz {

namespace {
const char* kTargets[] = { "", "ResidentMenu", "Entrance", "Flaunch",
                           "Set", "Psl", "MyPage", "Notification" };

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

std::string partLabel(const std::string& target) {
    if (target.empty()) return "themes/part_all"_i18n;
    return brls::getStr("themes/part_" + target);
}
} // namespace

ThemeBrowserActivity::ThemeBrowserActivity(IHttpClient* http) : http(http) {}

void ThemeBrowserActivity::onContentAvailable() {
    install_header_username(this);
    install_tls_warning_banner(this);

    if (auto* tp = this->getView("tabPacks")) {
        tp->registerClickAction([this](brls::View*) {
            this->packsMode = true; this->target = "";
            if (auto* pb = this->getView("partButton")) pb->setVisibility(brls::Visibility::GONE);
            brls::sync([this]() { this->reload(); });
            return true;
        });
        tp->addGestureRecognizer(new brls::TapGestureRecognizer(tp));
    }
    if (auto* tt = this->getView("tabThemes")) {
        tt->registerClickAction([this](brls::View*) {
            this->packsMode = false;
            if (auto* pb = this->getView("partButton")) pb->setVisibility(brls::Visibility::VISIBLE);
            brls::sync([this]() { this->reload(); });
            return true;
        });
        tt->addGestureRecognizer(new brls::TapGestureRecognizer(tt));
    }
    if (auto* sb = this->getView("searchButton")) {
        sb->registerClickAction([this](brls::View*) { this->openSearch(); return true; });
        sb->addGestureRecognizer(new brls::TapGestureRecognizer(sb));
    }
    if (auto* pb = this->getView("partButton")) {
        pb->registerClickAction([this](brls::View*) { this->cyclePart(); return true; });
        pb->addGestureRecognizer(new brls::TapGestureRecognizer(pb));
    }

    this->reload();
}

void ThemeBrowserActivity::reload() {
    this->runQuery(1);
}

void ThemeBrowserActivity::runQuery(int page) {
    if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::VISIBLE);

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
            if (auto* spinner = this->getView("spinner")) spinner->setVisibility(brls::Visibility::GONE);
            if (!results->first) { brls::Application::notify("themes/error_network"_i18n); return; }
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
    this->runAsync(
        [client, u, body, ok, cancelled]() {
            HttpRequest req;
            req.url       = u;
            req.cancelled = cancelled;
            HttpResponse r = client->request(req);
            if (r.ok()) { *body = r.body; *ok = true; }
        },
        [into, body, ok]() {
            if (*ok)
                into->setImageFromMem((const unsigned char*)body->data(), (int)body->size());
        });
}

void ThemeBrowserActivity::populate(const core::BrowsePage& pg) {
    auto* box = (brls::Box*)this->getView("resultsBox");
    auto* empty = (brls::Label*)this->getView("emptyLabel");
    if (!box) return;

    this->page       = pg.page;
    this->isComplete = pg.is_complete;

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
        more->registerClickAction([this](brls::View*) {
            int next = this->page + 1;
            brls::sync([this, next]() { this->runQuery(next); });
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
            brls::sync([this]() { this->reload(); });
        },
        "themes/search"_i18n, "themes/search_hint"_i18n, 64);
}

void ThemeBrowserActivity::cyclePart() {
    const int n = (int)(sizeof(kTargets) / sizeof(kTargets[0]));
    int cur = 0;
    for (int i = 0; i < n; i++) if (this->target == kTargets[i]) { cur = i; break; }
    this->target = kTargets[(cur + 1) % n];
    if (auto* lbl = (brls::Label*)this->getView("partButtonLabel"))
        lbl->setText("themes/filter_part"_i18n + std::string(": ") + partLabel(this->target));
    this->reload();
}

} // namespace thomaz
