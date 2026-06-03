#include "app/composer_activity.hpp"
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include "platform/feed/auth_store.hpp"

using namespace brls::literals;

namespace thomaz {

ComposerActivity::ComposerActivity(IFeedClient* client, IAlbumSource* album,
                                   ITitleService* titles, std::function<void()> onPosted)
    : client(client), album(album), titles(titles), onPosted(std::move(onPosted)) {}

ComposerActivity::~ComposerActivity() { *this->alive = false; }

void ComposerActivity::onContentAvailable()
{
    auto* captionCell = (brls::InputCell*)this->getView("captionCell");
    captionCell->init("thomaz/composer/caption"_i18n, "",
                      [this](std::string v){ this->caption = v; },
                      "thomaz/composer/caption"_i18n, "", 280);

    auto* postBtn = this->getView("postBtn");
    postBtn->registerClickAction([this](brls::View*) { this->doPost(); return true; });
    postBtn->addGestureRecognizer(new brls::TapGestureRecognizer(postBtn));

    this->loadAlbum();
}

std::string ComposerActivity::resolveGameName(std::uint64_t titleId)
{
    if (titleId == 0) return "";
    for (const auto& t : this->titles->listInstalled())
        if (t.title_id == titleId)
            return t.name;
    return "";
}

void ComposerActivity::loadAlbum()
{
    this->getView("albumSpinner")->setVisibility(brls::Visibility::VISIBLE);

    IAlbumSource* a = this->album;
    auto alive      = this->alive;

    brls::async([this, a, alive]() {
        auto entries = a->list();
        brls::sync([this, alive, entries]() {
            if (!alive->load()) return;
            this->getView("albumSpinner")->setVisibility(brls::Visibility::GONE);
            auto* grid = (brls::Box*)this->getView("albumGrid");
            if (entries.empty()) {
                this->getView("albumEmpty")->setVisibility(brls::Visibility::VISIBLE);
                return;
            }
            brls::Box* currentRow = nullptr;
            int col = 0;
            for (const auto& e : entries) {
                if (col % 2 == 0) {
                    currentRow = new brls::Box(brls::Axis::ROW);
                    currentRow->setWidth(brls::View::AUTO);
                    grid->addView(currentRow);
                }
                auto* cell = new brls::Image();
                cell->setWidth(120.0f);
                cell->setHeight(68.0f);
                cell->setCornerRadius(6.0f);
                cell->setMargins(6.0f, 6.0f, 6.0f, 6.0f);
                cell->setScalingType(brls::ImageScalingType::FILL);
                cell->setFocusable(true);
                if (!e.thumbnail.empty())
                    cell->setImageFromMem((unsigned char*)e.thumbnail.data(),
                                          (int)e.thumbnail.size());
                AlbumEntry entry = e;
                cell->registerClickAction([this, entry](brls::View*) {
                    this->selectEntry(entry); return true; });
                cell->addGestureRecognizer(new brls::TapGestureRecognizer(cell));
                currentRow->addView(cell);
                col++;
            }
        });
    });
}

void ComposerActivity::selectEntry(const AlbumEntry& entry)
{
    this->selectedEntryId = entry.id;
    this->selectedTitleId = entry.titleId;

    IAlbumSource* a = this->album;
    auto alive      = this->alive;
    std::string id  = entry.id;
    std::uint64_t tid = entry.titleId;

    this->selectedGameName = this->resolveGameName(tid);
    if (auto* gameLbl = (brls::Label*)this->getView("previewGame")) {
        gameLbl->setText(this->selectedGameName.empty()
            ? "" : ("thomaz/feed/game_tag"_i18n + this->selectedGameName));
    }

    this->selectedFullBytes.clear();
    brls::async([this, a, alive, id]() {
        auto bytes = a->loadFull(id);
        brls::sync([this, alive, id, bytes]() {
            if (!alive->load()) return;
            if (this->selectedEntryId != id) return;
            this->selectedFullBytes = bytes; // reused by doPost (avoids a 2nd load)
            if (auto* img = (brls::Image*)this->getView("previewImage"))
                if (!bytes.empty())
                    img->setImageFromMem((unsigned char*)bytes.data(), (int)bytes.size());
        });
    });
}

void ComposerActivity::doPost()
{
    if (this->busy) return;
    auto* status = (brls::Label*)this->getView("composerStatus");

    if (this->selectedEntryId.empty()) {
        status->setText("thomaz/composer/pick"_i18n);
        return;
    }

    this->busy = true;
    status->setText("thomaz/composer/posting"_i18n);

    auto sess = load_session();
    std::string token = sess ? sess->token : "";

    IFeedClient* c  = this->client;
    IAlbumSource* a = this->album;
    auto alive      = this->alive;
    std::string id  = this->selectedEntryId;
    std::string cap = this->caption;
    std::uint64_t tid = this->selectedTitleId;
    std::string game  = this->selectedGameName;
    std::vector<std::uint8_t> cached = this->selectedFullBytes; // copied on UI thread

    brls::async([this, c, a, alive, token, id, cap, tid, game, cached, status]() {
        // Reuse the bytes loaded for the preview; only hit the album again if the
        // cache is empty (e.g. preview load hadn't finished when Post was tapped).
        std::vector<std::uint8_t> jpeg = !cached.empty() ? cached : a->loadFull(id);
        ActionResult r = c->createPost(token, jpeg, cap, tid, game);
        brls::sync([this, alive, r, status]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!r.ok) {
                status->setText("thomaz/composer/post_failed"_i18n);
                return;
            }
            auto cb = this->onPosted;
            brls::Application::popActivity(brls::TransitionAnimation::NONE,
                                           [cb]() { if (cb) cb(); });
        });
    });
}

} // namespace thomaz
