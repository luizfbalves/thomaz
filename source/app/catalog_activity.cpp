#include "app/catalog_activity.hpp"
#include "app/app_header.hpp"
#include "app/catalog_detail_activity.hpp"
#include "app/tls_banner.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/ime.hpp>
#include <borealis/views/image.hpp>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "core/games/catalog.hpp"
#include "core/games/catalog_view.hpp"
#include "core/games/index_parse.hpp"
#include "core/games/title_id.hpp"
#include "platform/games/catalog_cache.hpp"
#include "platform/games/cover_art.hpp"
#include "platform/games/index_fetcher.hpp"
#include "platform/games/local_source.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {

constexpr NVGcolor kAccentBright = nvgRGB(0x92, 0x77, 0xFF);
constexpr NVGcolor kSurface2     = nvgRGB(0x22, 0x24, 0x2D);
constexpr NVGcolor kSurface3     = nvgRGB(0x2B, 0x2E, 0x39);
constexpr NVGcolor kAccentSoft   = nvgRGB(0x7C, 0x5C, 0xFF);
constexpr NVGcolor kGoodAdjacent = nvgRGB(0x57, 0xC9, 0x8A);

std::string human_size(std::uint64_t bytes) {
    if (bytes >= 1024ull * 1024ull * 1024ull)
        return std::to_string(bytes / (1024ull * 1024ull * 1024ull)) + " GB";
    if (bytes >= 1024ull * 1024ull)
        return std::to_string(bytes / (1024ull * 1024ull)) + " MB";
    if (bytes >= 1024ull)
        return std::to_string(bytes / 1024ull) + " KB";
    return std::to_string(bytes) + " B";
}

std::optional<int> version_from_url(const std::string& url) {
    for (std::size_t i = 0; i + 4 < url.size(); ++i) {
        if (url[i] == '[' && url[i + 1] == 'v') {
            std::size_t end = url.find(']', i + 2);
            if (end == std::string::npos)
                continue;
            try {
                return std::stoi(url.substr(i + 2, end - i - 2));
            } catch (...) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::string serialize_parsed_index(const thomaz::core::ParsedIndex& idx) {
    nlohmann::json j;
    nlohmann::json files = nlohmann::json::array();
    for (const auto& f : idx.files) {
        nlohmann::json fe;
        std::string url = f.url;
        if (!f.nameOverride.empty() && url.find('#') == std::string::npos)
            url += "#" + f.nameOverride;
        fe["url"]  = url;
        fe["size"] = f.size;
        files.push_back(fe);
    }
    j["files"]       = files;
    j["directories"] = nlohmann::json::array();
    return j.dump();
}

std::string kind_label_for_group(const thomaz::core::GroupedTitle& g) {
    for (const auto& r : g.rows) {
        if (r.kind == thomaz::core::TitleKind::Update) {
            const int ver = version_from_url(r.url).value_or(0);
            std::string tpl = "thomaz/catalog/kind_update"_i18n;
            const auto pos = tpl.find("{n}");
            if (pos != std::string::npos)
                tpl.replace(pos, 3, std::to_string(ver));
            return tpl;
        }
    }
    if (g.hasDlc)
        return "thomaz/catalog/kind_dlc"_i18n;
    return "thomaz/catalog/kind_base"_i18n;
}

NVGcolor kind_chip_bg(const thomaz::core::GroupedTitle& g) {
    if (g.hasUpdate)
        return kAccentSoft;
    if (g.hasDlc)
        return kGoodAdjacent;
    return kSurface3;
}

thomaz::core::TitleKind primary_kind(const thomaz::core::GroupedTitle& g) {
    if (g.hasUpdate)
        return thomaz::core::TitleKind::Update;
    if (g.hasDlc)
        return thomaz::core::TitleKind::Dlc;
    return thomaz::core::TitleKind::Base;
}

brls::Box* make_kind_chip(const std::string& text, NVGcolor bg) {
    auto* chip = new brls::Box(brls::Axis::ROW);
    chip->setBackgroundColor(bg);
    chip->setCornerRadius(8.0f);
    chip->setPadding(4.0f, 10.0f, 4.0f, 10.0f);
    chip->setMarginTop(6.0f);
    chip->setAlignItems(brls::AlignItems::CENTER);
    auto* lbl = new brls::Label();
    lbl->setText(text);
    lbl->setFontSize(13.0f);
    lbl->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    chip->addView(lbl);
    return chip;
}

} // namespace

CatalogActivity::CatalogActivity(ITitleService* titleService, IHttpClient* http,
                                   thomaz::core::SourceConfig source)
    : titleService(titleService), http(http), source(std::move(source)) {}

void CatalogActivity::onContentAvailable() {
    install_system_status(this);
    install_header_username(this);
    install_tls_warning_banner(this);

    if (auto* frame = this->getView("catalogFrame")) {
        if (is_local_source(this->source))
            frame->setTitle("thomaz/sources/local_sd"_i18n);
        else if (!this->source.label.empty())
            frame->setTitle(this->source.label);
    }

    this->bindChips();
    this->updateChipSelection();

    if (is_local_source(this->source)) {
        this->setLoading(true);
    } else if (auto cached = read_cached_index(this->source)) {
        this->hadCache   = true;
        this->allGrouped = this->loadFromBody(*cached);
        this->applyAndPopulate();
    } else {
        this->setLoading(true);
    }

    this->refreshFromNetwork();
}

void CatalogActivity::bindChips() {
    auto bind = [this](const char* id, auto fn) {
        if (auto* v = this->getView(id)) {
            v->registerClickAction([this, fn, alive = this->alive](brls::View*) {
                fn();
                brls::sync([this, alive]() {
                    if (!alive->load()) return;
                    this->updateChipSelection();
                    this->applyAndPopulate();
                });
                return true;
            });
            v->addGestureRecognizer(new brls::TapGestureRecognizer(v));
        }
    };

    bind("viewGrid", [this]() { this->gridMode = true; });
    bind("viewList", [this]() { this->gridMode = false; });
    bind("sortName", [this]() { this->viewQuery.sort = thomaz::core::CatalogSort::NameAsc; });
    bind("sortSize", [this]() { this->viewQuery.sort = thomaz::core::CatalogSort::SizeDesc; });
    bind("filterUpdate", [this]() {
        this->viewQuery.filter = (this->viewQuery.filter == thomaz::core::CatalogFilter::HasUpdate)
                                     ? thomaz::core::CatalogFilter::All
                                     : thomaz::core::CatalogFilter::HasUpdate;
    });
    bind("filterDlc", [this]() {
        this->viewQuery.filter = (this->viewQuery.filter == thomaz::core::CatalogFilter::HasDlc)
                                     ? thomaz::core::CatalogFilter::All
                                     : thomaz::core::CatalogFilter::HasDlc;
    });
    bind("filterBase", [this]() {
        this->viewQuery.filter = (this->viewQuery.filter == thomaz::core::CatalogFilter::BaseOnly)
                                     ? thomaz::core::CatalogFilter::All
                                     : thomaz::core::CatalogFilter::BaseOnly;
    });

    if (auto* sb = this->getView("searchButton")) {
        sb->registerClickAction([this](brls::View*) { this->openSearch(); return true; });
        sb->addGestureRecognizer(new brls::TapGestureRecognizer(sb));
    }
}

void CatalogActivity::updateChipSelection() {
    const auto paint = [this](const char* id, bool active) {
        if (auto* b = (brls::Box*)this->getView(id))
            b->setBackgroundColor(active ? kAccentBright : kSurface2);
    };

    paint("viewGrid", this->gridMode);
    paint("viewList", !this->gridMode);
    paint("sortName", this->viewQuery.sort == thomaz::core::CatalogSort::NameAsc);
    paint("sortSize", this->viewQuery.sort == thomaz::core::CatalogSort::SizeDesc);
    paint("filterUpdate", this->viewQuery.filter == thomaz::core::CatalogFilter::HasUpdate);
    paint("filterDlc", this->viewQuery.filter == thomaz::core::CatalogFilter::HasDlc);
    paint("filterBase", this->viewQuery.filter == thomaz::core::CatalogFilter::BaseOnly);
}

void CatalogActivity::setLoading(bool on) {
    if (auto* lb = this->getView("loadingBox"))
        lb->setVisibility(on ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    if (on) {
        if (auto* rb = this->getView("resultsBox")) rb->setVisibility(brls::Visibility::GONE);
        if (auto* el = this->getView("emptyLabel")) el->setVisibility(brls::Visibility::GONE);
    }
}

void CatalogActivity::showStatusNote(const std::string& text) {
    if (auto* note = (brls::Label*)this->getView("statusNote")) {
        if (text.empty()) {
            note->setVisibility(brls::Visibility::GONE);
        } else {
            note->setText(text);
            note->setVisibility(brls::Visibility::VISIBLE);
        }
    }
}

std::vector<thomaz::core::GroupedTitle> CatalogActivity::loadFromBody(const std::string& body) {
    thomaz::core::ParsedIndex idx = thomaz::core::parse_index(body);
    if (!idx.ok)
        return {};
    return thomaz::core::group_catalog(idx);
}

void CatalogActivity::applyAndPopulate() {
    const auto view = thomaz::core::apply_view(this->allGrouped, this->viewQuery);
    this->populate(view);
}

void CatalogActivity::refreshFromNetwork() {
    if (is_local_source(this->source)) {
        this->setLoading(true);
        auto result = std::make_shared<FetchedCatalog>();
        this->runAsync(
            [result]() {
                bool truncated = false;
                result->merged = scan_local_files(&truncated);
                result->truncated = truncated;
                result->ok        = result->merged.ok;
            },
            [this, result]() {
                this->setLoading(false);
                if (!result->ok) {
                    if (auto* el = (brls::Label*)this->getView("emptyLabel")) {
                        el->setText("thomaz/catalog/empty"_i18n);
                        el->setVisibility(brls::Visibility::VISIBLE);
                    }
                    if (auto* rb = this->getView("resultsBox"))
                        rb->setVisibility(brls::Visibility::GONE);
                    return;
                }
                this->hadCache   = true;
                this->truncated  = result->truncated;
                this->allGrouped = thomaz::core::group_catalog(result->merged);
                write_cached_index(this->source, serialize_parsed_index(result->merged));
                if (this->truncated)
                    this->showStatusNote("thomaz/catalog/truncated_note"_i18n);
                else
                    this->showStatusNote("");
                this->applyAndPopulate();
            });
        return;
    }

    if (!this->hadCache)
        this->setLoading(true);
    else
        this->showStatusNote("");

    IHttpClient* client = this->http;
    thomaz::core::SourceConfig cfg = this->source;
    auto result     = std::make_shared<FetchedCatalog>();
    auto cancelled  = this->cancelledFlag();
    const bool cache = this->hadCache;

    this->runAsync(
        [client, cfg, result, cancelled]() { *result = fetch_index(client, cfg, cancelled); },
        [this, result, cache]() {
            this->setLoading(false);

            if (!result->ok) {
                if (cache) {
                    this->showStatusNote("thomaz/catalog/offline_note"_i18n);
                    return;
                }
                this->showStatusNote("");
                if (auto* el = (brls::Label*)this->getView("emptyLabel")) {
                    std::string msg = "thomaz/sources/error_fetch"_i18n;
                    if (result->error.find("too large") != std::string::npos)
                        msg = "thomaz/sources/error_too_large"_i18n;
                    else if (result->error.find("401") != std::string::npos ||
                             result->error.find("403") != std::string::npos)
                        msg = "thomaz/sources/error_auth"_i18n;
                    el->setText(msg);
                    el->setVisibility(brls::Visibility::VISIBLE);
                }
                if (auto* rb = this->getView("resultsBox")) rb->setVisibility(brls::Visibility::GONE);
                return;
            }

            this->hadCache   = true;
            this->truncated  = result->truncated;
            this->allGrouped = thomaz::core::group_catalog(result->merged);
            write_cached_index(this->source, serialize_parsed_index(result->merged));

            if (this->truncated)
                this->showStatusNote("thomaz/catalog/truncated_note"_i18n);
            else
                this->showStatusNote("");

            this->applyAndPopulate();
        });
}

void CatalogActivity::loadCover(std::uint64_t titleId, thomaz::core::TitleKind kind,
                                brls::Image* into) {
    if (!into) return;
    IHttpClient* client = this->http;
    ITitleService* titles = this->titleService;
    auto art       = std::make_shared<CoverArt>();
    auto cancelled = this->cancelledFlag();
    auto gen       = this->listGen;
    const auto genAtDispatch = gen->load();

    this->runAsync(
        [client, titles, titleId, kind, art, cancelled]() {
            *art = resolve_cover(client, titles, titleId, kind, cancelled);
        },
        [into, art, gen, genAtDispatch]() {
            if (gen->load() != genAtDispatch) return;
            if (!art->ok || art->bytes.empty()) return;
            into->setImageFromMem(reinterpret_cast<const unsigned char*>(art->bytes.data()),
                                  static_cast<int>(art->bytes.size()));
        });
}

void CatalogActivity::populate(const std::vector<thomaz::core::GroupedTitle>& view) {
    auto* box   = (brls::Box*)this->getView("resultsBox");
    auto* empty = (brls::Label*)this->getView("emptyLabel");
    if (!box) return;

    this->listGen->fetch_add(1);

    box->setVisibility(brls::Visibility::VISIBLE);
    box->clearViews();

    if (view.empty()) {
        if (empty) {
            empty->setText(this->allGrouped.empty() ? "thomaz/catalog/empty"_i18n
                                                    : "thomaz/catalog/no_matches"_i18n);
            empty->setVisibility(brls::Visibility::VISIBLE);
        }
        return;
    }
    if (empty) empty->setVisibility(brls::Visibility::GONE);

    brls::Box* rowBox = nullptr;
    int col = 0;

    for (const auto& entry : view) {
        thomaz::core::GroupedTitle g = entry;
        const auto kind = primary_kind(g);
        const std::uint64_t artId = g.baseId != 0 ? g.baseId : (g.rows.empty() ? 0 : g.rows.front().titleId);

        brls::Box* card = nullptr;
        if (this->gridMode) {
            if (col == 0) {
                rowBox = new brls::Box(brls::Axis::ROW);
                rowBox->setMarginBottom(12.0f);
                box->addView(rowBox);
            }

            card = new brls::Box(brls::Axis::COLUMN);
            card->setWidth(384.0f);
            card->setMarginRight(12.0f);
            card->setPadding(8.0f, 8.0f, 10.0f, 8.0f);
            card->setCornerRadius(12.0f);
            card->setFocusable(true);
            card->setHideHighlightBackground(true);
            card->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23));

            auto* img = new brls::Image();
            img->setWidth(368.0f);
            img->setHeight(207.0f);
            img->setCornerRadius(8.0f);
            card->addView(img);
            if (artId != 0)
                this->loadCover(artId, kind, img);

            auto* name = new brls::Label();
            name->setText(g.displayName);
            name->setFontSize(15.0f);
            name->setLineHeight(1.2f);
            name->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
            name->setMarginTop(8.0f);
            card->addView(name);

            auto* meta = new brls::Label();
            meta->setText(human_size(g.totalSize));
            meta->setFontSize(13.0f);
            meta->setMarginTop(4.0f);
            meta->setTextColor(nvgRGB(0xC8, 0xC8, 0xD0));
            card->addView(meta);

            card->addView(make_kind_chip(kind_label_for_group(g), kind_chip_bg(g)));
            rowBox->addView(card);
            col = (col + 1) % 3;
        } else {
            card = new brls::Box(brls::Axis::ROW);
            card->setHeight(56.0f);
            card->setMarginBottom(8.0f);
            card->setPadding(12.0f, 16.0f, 12.0f, 16.0f);
            card->setCornerRadius(8.0f);
            card->setFocusable(true);
            card->setHideHighlightBackground(true);
            card->setAlignItems(brls::AlignItems::CENTER);
            card->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23));

            auto* name = new brls::Label();
            name->setText(g.displayName);
            name->setFontSize(15.0f);
            name->setGrow(1.0f);
            name->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
            card->addView(name);

            auto* sizeLbl = new brls::Label();
            sizeLbl->setText(human_size(g.totalSize));
            sizeLbl->setFontSize(13.0f);
            sizeLbl->setMarginRight(12.0f);
            sizeLbl->setTextColor(nvgRGB(0xC8, 0xC8, 0xD0));
            card->addView(sizeLbl);

            auto* chip = make_kind_chip(kind_label_for_group(g), kind_chip_bg(g));
            chip->setMarginTop(0.0f);
            card->addView(chip);
            box->addView(card);
        }

        card->registerClickAction([this, g](brls::View*) {
            brls::Application::pushActivity(
                new CatalogDetailActivity(this->http, g, this->titleService));
            return true;
        });
        card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
    }

    this->claimInitialFocus(box);
}

void CatalogActivity::openSearch() {
    brls::Application::getPlatform()->getImeManager()->openForText(
        [this, alive = this->alive](std::string q) {
            if (!alive->load()) return;
            this->viewQuery.search = q;
            brls::sync([this, alive]() {
                if (!alive->load()) return;
                this->applyAndPopulate();
            });
        },
        "thomaz/catalog/search"_i18n, "thomaz/catalog/search_hint"_i18n, 64);
}

} // namespace thomaz
