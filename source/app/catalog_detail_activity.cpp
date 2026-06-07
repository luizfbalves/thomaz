#include "app/catalog_detail_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/views/image.hpp>
#include <optional>
#include <string>

#include "core/games/title_id.hpp"
#include "platform/games/cover_art.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {

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

std::string row_kind_label(const thomaz::core::CatalogRow& row) {
    switch (row.kind) {
    case thomaz::core::TitleKind::Base:
        return "thomaz/catalog/kind_base"_i18n;
    case thomaz::core::TitleKind::Update: {
        const int ver = version_from_url(row.url).value_or(0);
        std::string tpl = "thomaz/catalog/kind_update"_i18n;
        const auto pos = tpl.find("{n}");
        if (pos != std::string::npos)
            tpl.replace(pos, 3, std::to_string(ver));
        return tpl;
    }
    case thomaz::core::TitleKind::Dlc:
        return "thomaz/catalog/kind_dlc"_i18n;
    case thomaz::core::TitleKind::Unknown:
    default:
        return row.nameOverride.empty() ? "?" : row.nameOverride;
    }
}

NVGcolor row_kind_bg(thomaz::core::TitleKind kind) {
    switch (kind) {
    case thomaz::core::TitleKind::Update:
        return kAccentSoft;
    case thomaz::core::TitleKind::Dlc:
        return kGoodAdjacent;
    case thomaz::core::TitleKind::Base:
    default:
        return kSurface3;
    }
}

} // namespace

CatalogDetailActivity::CatalogDetailActivity(IHttpClient* http,
                                               thomaz::core::GroupedTitle title,
                                               ITitleService* titleService)
    : grouped(std::move(title)), http(http), titleService(titleService) {}

void CatalogDetailActivity::onContentAvailable() {
    install_system_status(this);
    install_header_username(this);
    install_tls_warning_banner(this);

    if (auto* name = (brls::Label*)this->getView("detailName"))
        name->setText(this->grouped.displayName);
    if (auto* rl = (brls::Label*)this->getView("rowsLabel"))
        rl->setText(this->grouped.displayName);

    this->buildRows();

    if (auto* status = this->getView("detailStatus"))
        status->setVisibility(brls::Visibility::VISIBLE);

    const std::uint64_t artId =
        this->grouped.baseId != 0
            ? this->grouped.baseId
            : (this->grouped.rows.empty() ? 0 : this->grouped.rows.front().titleId);

    IHttpClient* client = this->http;
    ITitleService* titles = this->titleService;
    auto art       = std::make_shared<CoverArt>();
    auto cancelled = this->cancelledFlag();
    thomaz::core::TitleKind kind = thomaz::core::TitleKind::Base;

    this->runAsync(
        [client, titles, artId, kind, art, cancelled]() {
            if (artId != 0)
                *art = resolve_cover(client, titles, artId, kind, cancelled);
            else
                art->ok = true;
        },
        [this, art]() {
            if (art->ok && !art->bytes.empty()) {
                if (auto* hero = (brls::Image*)this->getView("detailPreview"))
                    hero->setImageFromMem(
                        reinterpret_cast<const unsigned char*>(art->bytes.data()),
                        static_cast<int>(art->bytes.size()));
            }
            this->onHeroReady();
        });
}

void CatalogDetailActivity::onHeroReady() {
    if (auto* status = this->getView("detailStatus"))
        status->setVisibility(brls::Visibility::GONE);
    if (auto* content = this->getView("detailContent"))
        content->setVisibility(brls::Visibility::VISIBLE);

    this->claimInitialFocus((brls::Box*)this->getView("rowsBox"));
}

void CatalogDetailActivity::buildRows() {
    auto* box = (brls::Box*)this->getView("rowsBox");
    if (!box) return;
    box->clearViews();

    for (const auto& row : this->grouped.rows) {
        auto* rowBox = new brls::Box(brls::Axis::ROW);
        rowBox->setHeight(48.0f);
        rowBox->setMarginBottom(8.0f);
        rowBox->setPadding(10.0f, 14.0f, 10.0f, 14.0f);
        rowBox->setCornerRadius(8.0f);
        rowBox->setAlignItems(brls::AlignItems::CENTER);
        rowBox->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23));
        rowBox->setFocusable(true);
        rowBox->setHideHighlightBackground(true);

        auto* chip = new brls::Box(brls::Axis::ROW);
        chip->setBackgroundColor(row_kind_bg(row.kind));
        chip->setCornerRadius(8.0f);
        chip->setPadding(4.0f, 10.0f, 4.0f, 10.0f);
        chip->setMarginRight(12.0f);
        auto* chipLbl = new brls::Label();
        chipLbl->setText(row_kind_label(row));
        chipLbl->setFontSize(13.0f);
        chipLbl->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
        chip->addView(chipLbl);
        rowBox->addView(chip);

        auto* sizeLbl = new brls::Label();
        sizeLbl->setText(human_size(row.size));
        sizeLbl->setFontSize(15.0f);
        sizeLbl->setGrow(1.0f);
        sizeLbl->setTextColor(nvgRGB(0xC8, 0xC8, 0xD0));
        rowBox->addView(sizeLbl);

        box->addView(rowBox);
    }
}

} // namespace thomaz
