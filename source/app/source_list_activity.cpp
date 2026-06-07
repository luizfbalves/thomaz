#include "app/source_list_activity.hpp"
#include "app/app_header.hpp"
#include "app/catalog_activity.hpp"
#include "app/tls_banner.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/ime.hpp>
#include <cctype>
#include <iomanip>
#include <sstream>

#include "platform/app_settings.hpp"
#include "platform/feed/auth_store.hpp"
#include "platform/games/http_source_sync_client.hpp"
#include "platform/games/index_fetch_util.hpp"
#include "platform/games/local_source.hpp"
#include "platform/games/source_store.hpp"
#include "platform/saves/cloud_save_client.hpp"

namespace thomaz {

namespace {

constexpr NVGcolor kAccentBright = nvgRGB(0x92, 0x77, 0xFF);
constexpr NVGcolor kSurface2     = nvgRGB(0x22, 0x24, 0x2D);
constexpr NVGcolor kGood         = nvgRGB(0x57, 0xC9, 0x8A);
constexpr NVGcolor kWarning      = nvgRGB(0xC0, 0x3A, 0x3A);

bool is_http_url(const std::string& url) {
    const auto lower = [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };
    if (url.size() < 8)
        return false;
    return (lower(url[0]) == 'h' && lower(url[1]) == 't' && lower(url[2]) == 't' &&
            lower(url[3]) == 'p' && url[4] == ':' && url[5] == '/' && url[6] == '/') ||
           (url.size() >= 9 && lower(url[0]) == 'h' && lower(url[1]) == 't' &&
            lower(url[2]) == 't' && lower(url[3]) == 'p' && lower(url[4]) == 's' &&
            url[5] == ':' && url[6] == '/' && url[7] == '/');
}

bool has_basic_in_url(const std::string& url) {
    const auto scheme = url.find("://");
    if (scheme == std::string::npos)
        return false;
    const std::size_t hostStart = scheme + 3;
    const auto at               = url.find('@', hostStart);
    const auto colon            = url.find(':', hostStart);
    return at != std::string::npos && colon != std::string::npos && colon < at;
}

std::string stable_remote_id(const std::string& url) {
    std::uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : url) {
        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ULL;
    }
    std::ostringstream os;
    os << "sl" << std::hex << std::setfill('0') << std::setw(16) << h;
    return os.str();
}

brls::Box* make_row_button(const std::string& text, NVGcolor bg,
                           std::function<bool(brls::View*)> onClick) {
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setHeight(48.0f);
    row->setFocusable(true);
    row->setMarginBottom(8.0f);
    row->setCornerRadius(10.0f);
    row->setPadding(12.0f, 16.0f, 12.0f, 16.0f);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setHideHighlightBackground(true);
    row->setBackgroundColor(bg);
    auto* lbl = new brls::Label();
    lbl->setText(text);
    lbl->setFontSize(16.0f);
    lbl->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    lbl->setGrow(1.0f);
    row->addView(lbl);
    row->registerClickAction([onClick](brls::View* v) { return onClick(v); });
    row->addGestureRecognizer(new brls::TapGestureRecognizer(row));
    return row;
}

} // namespace

SourceListActivity::SourceListActivity(ITitleService* titleService, IHttpClient* http)
    : titleService(titleService), http(http) {}

void SourceListActivity::onContentAvailable() {
    install_system_status(this);
    install_header_username(this);
    install_tls_warning_banner(this);

    if (auto* add = this->getView("addSourceButton")) {
        add->registerClickAction([this](brls::View*) {
            this->beginAddSource();
            return true;
        });
        add->addGestureRecognizer(new brls::TapGestureRecognizer(add));
    }

    if (auto* sync = this->getView("syncButton")) {
        sync->registerClickAction([this](brls::View*) {
            this->doSync();
            return true;
        });
        sync->addGestureRecognizer(new brls::TapGestureRecognizer(sync));
    }

    this->reload();
}

void SourceListActivity::reload() {
    this->sources = load_sources();
    this->populate();
}

std::string SourceListActivity::rowLabel(const thomaz::core::SourceConfig& cfg) const {
    if (is_local_source(cfg))
        return "thomaz/sources/local_sd"_i18n;
    if (!cfg.label.empty())
        return cfg.label;
    return redacted_host_from_url(cfg.url);
}

void SourceListActivity::populate() {
    auto* box = (brls::Box*)this->getView("resultsBox");
    if (!box)
        return;

    box->clearViews();

    const bool emptyRemotes = this->sources.empty();
    if (auto* eh = this->getView("emptyHeading"))
        eh->setVisibility(emptyRemotes ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (auto* eb = this->getView("emptyBody"))
        eb->setVisibility(emptyRemotes ? brls::Visibility::VISIBLE : brls::Visibility::GONE);

    brls::View* firstFocus = nullptr;

    // Local SD peer is always shown (SRC-03).
    {
        const auto localCfg = make_local_peer_config();
        auto* row           = make_row_button(this->rowLabel(localCfg), kSurface2,
                                              [this, localCfg](brls::View*) {
                                                  this->openSource(localCfg);
                                                  return true;
                                              });
        box->addView(row);
        if (!firstFocus)
            firstFocus = emptyRemotes ? this->getView("addSourceButton") : row;
    }

    for (std::size_t i = 0; i < this->sources.size(); ++i) {
        const std::size_t idx = i;
        const auto cfg        = this->sources[i];

        auto* rowWrap = new brls::Box(brls::Axis::COLUMN);
        rowWrap->setMarginBottom(4.0f);

        auto* openRow = make_row_button(this->rowLabel(cfg), kSurface2,
                                        [this, cfg](brls::View*) {
                                            this->openSource(cfg);
                                            return true;
                                        });
        rowWrap->addView(openRow);

        auto* removeRow = make_row_button("Remove", kWarning, [this, idx](brls::View*) {
            this->confirmRemove(idx);
            return true;
        });
        removeRow->setHeight(40.0f);
        removeRow->setMarginBottom(8.0f);
        rowWrap->addView(removeRow);

        box->addView(rowWrap);
        if (!firstFocus && idx == 0)
            firstFocus = openRow;
    }

    box->setVisibility(brls::Visibility::VISIBLE);
    this->claimInitialFocus(firstFocus ? firstFocus : box);

    if (emptyRemotes) {
        if (auto* add = this->getView("addSourceButton"))
            this->claimInitialFocus(add);
    }
}

void SourceListActivity::setAddBusy(bool on) {
    this->addBusy = on;
    if (auto* add = (brls::Box*)this->getView("addSourceButton")) {
        add->setFocusable(!on);
        add->setAlpha(on ? 0.5f : 1.0f);
    }
}

void SourceListActivity::setSyncBusy(bool on) {
    this->syncBusy = on;
    if (auto* sync = (brls::Box*)this->getView("syncButton")) {
        sync->setFocusable(!on);
        sync->setAlpha(on ? 0.5f : 1.0f);
    }
}

void SourceListActivity::updateSyncAppearance(bool synced) {
    this->syncOk = synced;
    if (auto* sync = (brls::Box*)this->getView("syncButton"))
        sync->setBackgroundColor(synced ? kGood : kSurface2);
    if (auto* lbl = (brls::Label*)this->getView("syncLabel"))
        lbl->setText(synced ? "thomaz/sources/sync_ok"_i18n : "thomaz/sources/sync"_i18n);
}

void SourceListActivity::openSource(const thomaz::core::SourceConfig& cfg) {
    brls::Application::pushActivity(
        new CatalogActivity(this->titleService, this->http, cfg),
        brls::TransitionAnimation::SLIDE_LEFT);
}

void SourceListActivity::beginAddSource() {
    if (this->addBusy)
        return;

    brls::Application::getPlatform()->getImeManager()->openForText(
        [this, alive = this->alive](std::string url) {
            if (!alive->load())
                return;
            url.erase(0, url.find_first_not_of(" \t\r\n"));
            url.erase(url.find_last_not_of(" \t\r\n") + 1);
            if (url.empty())
                return;
            if (!is_http_url(url)) {
                brls::Application::notify("thomaz/sources/error_fetch"_i18n);
                return;
            }
            brls::sync([this, alive, url]() {
                if (!alive->load())
                    return;
                if (has_basic_in_url(url)) {
                    this->finishAddSource(url, thomaz::core::SourceAuthType::BasicInUrl, "");
                } else {
                    this->promptAuthForUrl(url);
                }
            });
        },
        "thomaz/sources/add"_i18n, "https://", 512);
}

void SourceListActivity::promptAuthForUrl(const std::string& url) {
    auto* dialog = new brls::Dialog(
        "Optional: add a custom header (Name: value) or referrer URL on the next screen, or skip.");
    dialog->addButton("Skip", [this, alive = this->alive, url]() {
        if (!alive->load())
            return;
        this->finishAddSource(url, thomaz::core::SourceAuthType::None, "");
    });
    dialog->addButton("Header", [this, alive = this->alive, url]() {
        if (!alive->load())
            return;
        brls::Application::getPlatform()->getImeManager()->openForText(
            [this, alive, url](std::string secret) {
                if (!alive->load())
                    return;
                brls::sync([this, alive, url, secret]() {
                    if (!alive->load())
                        return;
                    this->finishAddSource(url, thomaz::core::SourceAuthType::Header, secret);
                });
            },
            "Header", "Name: value", 256);
    });
    dialog->addButton("Referrer", [this, alive = this->alive, url]() {
        if (!alive->load())
            return;
        brls::Application::getPlatform()->getImeManager()->openForText(
            [this, alive, url](std::string secret) {
                if (!alive->load())
                    return;
                brls::sync([this, alive, url, secret]() {
                    if (!alive->load())
                        return;
                    this->finishAddSource(url, thomaz::core::SourceAuthType::Referrer, secret);
                });
            },
            "Referrer", "https://", 512);
    });
    dialog->open();
}

void SourceListActivity::finishAddSource(const std::string& url,
                                         thomaz::core::SourceAuthType auth,
                                         const std::string& secret) {
    if (this->addBusy)
        return;
    this->setAddBusy(true);

    thomaz::core::SourceConfig cfg;
    cfg.url        = url;
    cfg.authType   = auth;
    cfg.authSecret = secret;
    cfg.label      = redacted_host_from_url(url);

    this->sources.push_back(cfg);
    const bool ok = save_sources(this->sources);
    this->setAddBusy(false);

    if (!ok) {
        this->sources.pop_back();
        brls::Application::notify("thomaz/sources/error_fetch"_i18n);
        return;
    }

    this->syncOk = false;
    this->updateSyncAppearance(false);
    this->populate();
}

void SourceListActivity::doSync() {
    if (this->syncBusy)
        return;

    auto sess = load_session();
    if (!sess || sess->token.empty()) {
        brls::Application::notify("thomaz/sources/sync_auth_expired"_i18n);
        return;
    }

    this->setSyncBusy(true);
    this->updateSyncAppearance(false);

    IHttpClient* client = this->http;
    std::string token   = sess->token;
    std::string base    = load_api_base_url();
    auto sourcesSnap    = std::make_shared<std::vector<thomaz::core::SourceConfig>>(this->sources);
    auto results        = std::make_shared<std::vector<SourceSyncResult>>();
    auto hadAuthExpired = std::make_shared<bool>(false);
    auto cancelled      = this->cancelledFlag();

    this->runAsync(
        [client, token, base, sourcesSnap, results, hadAuthExpired, cancelled]() {
            HttpSourceSyncClient sync(client, base);
            results->reserve(sourcesSnap->size());
            for (const auto& cfg : *sourcesSnap) {
                if (is_local_source(cfg))
                    continue;
                std::string id = cfg.remoteId.empty() ? stable_remote_id(cfg.url) : cfg.remoteId;
                SourceSyncResult r = sync.push(token, id, cfg, cancelled);
                if (r.error == kCloudAuthExpired)
                    *hadAuthExpired = true;
                results->push_back(std::move(r));
            }
        },
        [this, sourcesSnap, results, hadAuthExpired]() {
            this->setSyncBusy(false);

            if (*hadAuthExpired) {
                brls::Application::notify("thomaz/sources/sync_auth_expired"_i18n);
                return;
            }

            bool allOk = true;
            std::size_t ri = 0;
            for (std::size_t i = 0; i < sourcesSnap->size(); ++i) {
                if (is_local_source((*sourcesSnap)[i]))
                    continue;
                if (ri >= results->size() || !(*results)[ri].ok) {
                    allOk = false;
                    ++ri;
                    continue;
                }
                this->sources[i].remoteId = (*results)[ri].id;
                ++ri;
            }

            if (!allOk) {
                brls::Application::notify("thomaz/sources/error_fetch"_i18n);
                return;
            }

            save_sources(this->sources);
            this->updateSyncAppearance(true);
        });
}

void SourceListActivity::confirmRemove(std::size_t index) {
    if (index >= this->sources.size())
        return;

    auto* dialog = new brls::Dialog("thomaz/sources/remove_confirm"_i18n);
    dialog->addButton("thomaz/common/cancel"_i18n, []() {});
    dialog->addButton("Remove", [this, alive = this->alive, index]() {
        if (!alive->load())
            return;
        if (index >= this->sources.size())
            return;

        thomaz::core::SourceConfig removed = this->sources[index];
        this->sources.erase(this->sources.begin() + static_cast<std::ptrdiff_t>(index));
        save_sources(this->sources);
        this->syncOk = false;
        this->updateSyncAppearance(false);
        this->populate();

        if (removed.remoteId.empty())
            return;
        auto sess = load_session();
        if (!sess || sess->token.empty())
            return;

        IHttpClient* client = this->http;
        std::string token   = sess->token;
        std::string base    = load_api_base_url();
        std::string rid     = removed.remoteId;
        auto cancelled      = this->cancelledFlag();

        this->runAsync(
            [client, token, base, rid, cancelled]() {
                HttpSourceSyncClient sync(client, base);
                (void)sync.remove(token, rid, cancelled);
            },
            []() {});
    });
    dialog->open();
}

} // namespace thomaz
