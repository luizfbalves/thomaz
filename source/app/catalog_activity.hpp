#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <borealis.hpp>

#include "app/thomaz_activity.hpp"
#include "core/games/catalog.hpp"
#include "core/games/catalog_view.hpp"
#include "core/games/source_link.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

// Browses a linked content source: cache-first grouped cover-art grid/list with
// search, sort, and content-filter chips. Detail push is read-only (no install).
class CatalogActivity : public ThomazActivity {
  public:
    CatalogActivity(ITitleService* titleService, IHttpClient* http,
                    thomaz::core::SourceConfig source);

    CONTENT_FROM_XML_RES("activity/catalog.xml");
    void onContentAvailable() override;

  private:
    void bindChips();
    void updateChipSelection();
    void applyAndPopulate();
    void populate(const std::vector<thomaz::core::GroupedTitle>& view);
    void loadCover(std::uint64_t titleId, thomaz::core::TitleKind kind, brls::Image* into);
    void openSearch();
    void refreshFromNetwork();
    void showStatusNote(const std::string& text);
    void setLoading(bool on);

    std::vector<thomaz::core::GroupedTitle> loadFromBody(const std::string& body);

    ITitleService*              titleService;
    IHttpClient*                http;
    thomaz::core::SourceConfig  source;

    std::vector<thomaz::core::GroupedTitle> allGrouped;
    thomaz::core::CatalogViewQuery          viewQuery;
    bool                                    gridMode     = true;
    bool                                    hadCache     = false;
    bool                                    truncated    = false;

    std::shared_ptr<std::atomic<std::uint64_t>> listGen =
        std::make_shared<std::atomic<std::uint64_t>>(0);
};

} // namespace thomaz
