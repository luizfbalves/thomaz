#include "app/feed_activity.hpp"
#include "app/app_header.hpp"
#include "app/composer_activity.hpp"
#include "app/auth_activity.hpp"
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include "core/feed/feed_pagination.hpp"
#include "platform/feed/auth_store.hpp"

using namespace brls::literals;

namespace thomaz {

FeedActivity::FeedActivity(IFeedClient* client, IAlbumSource* album, ITitleService* titles)
    : client(client), album(album), titles(titles) {}

FeedActivity::~FeedActivity() { *this->alive = false; }

void FeedActivity::onContentAvailable()
{
    install_header_username(this);

    auto* compose = this->getView("composeBtn");
    compose->registerClickAction([this](brls::View*) { this->onComposePressed(); return true; });
    compose->addGestureRecognizer(new brls::TapGestureRecognizer(compose));

    auto* retry = this->getView("feedRetry");
    retry->registerClickAction([this](brls::View*) { this->loadFirstPage(); return true; });
    retry->addGestureRecognizer(new brls::TapGestureRecognizer(retry));

    this->loadFirstPage();
}

void FeedActivity::loadFirstPage()
{
    this->posts.clear();
    this->nextCursor.clear();
    if (auto* box = (brls::Box*)this->getView("feedListBox")) box->clearViews();
    this->getView("feedError")->setVisibility(brls::Visibility::GONE);
    this->getView("feedEmpty")->setVisibility(brls::Visibility::GONE);
    this->getView("feedSplit")->setVisibility(brls::Visibility::GONE);
    this->getView("feedSpinner")->setVisibility(brls::Visibility::VISIBLE);

    IFeedClient* c = this->client;
    auto alive     = this->alive;

    brls::async([this, c, alive]() {
        feed::FeedPage page = c->fetchFeed("");
        brls::sync([this, alive, page]() {
            if (!alive->load()) return;
            this->getView("feedSpinner")->setVisibility(brls::Visibility::GONE);

            bool transportFail = !page.ok; // empty-but-ok feed shows the empty state, not an error
            size_t before = this->posts.size();
            this->hasMore = feed::merge_feed_page(this->posts, page);
            this->nextCursor = page.nextCursor;

            if (this->posts.empty()) {
                if (transportFail)
                    this->getView("feedError")->setVisibility(brls::Visibility::VISIBLE);
                else
                    this->getView("feedEmpty")->setVisibility(brls::Visibility::VISIBLE);
                return;
            }
            this->getView("feedSplit")->setVisibility(brls::Visibility::VISIBLE);
            this->renderNewRows(before);
            this->showDetail(this->posts.front().id);
        });
    });
}

void FeedActivity::loadNextPage()
{
    if (this->loading || !this->hasMore) return;
    this->loading = true;

    IFeedClient* c = this->client;
    auto alive     = this->alive;
    std::string cur = this->nextCursor;

    brls::async([this, c, alive, cur]() {
        feed::FeedPage page = c->fetchFeed(cur);
        brls::sync([this, alive, page]() {
            if (!alive->load()) return;
            this->loading = false;
            size_t before = this->posts.size();
            this->hasMore = feed::merge_feed_page(this->posts, page);
            this->nextCursor = page.nextCursor;
            this->renderNewRows(before);
        });
    });
}

void FeedActivity::renderNewRows(size_t fromIndex)
{
    auto* listBox = (brls::Box*)this->getView("feedListBox");
    if (!listBox) return;

    for (size_t i = fromIndex; i < this->posts.size(); ++i) {
        const feed::Post& post = this->posts[i];
        std::string id = post.id;

        auto* row = new brls::Box(brls::Axis::COLUMN);
        row->setFocusable(true);
        row->setMarginBottom(8.0f);
        row->setPadding(10.0f, 12.0f, 10.0f, 12.0f);
        row->setCornerRadius(12.0f);
        row->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23));

        auto* user = new brls::Label();
        user->setText("@" + post.author.username);
        user->setFontSize(15.0f);
        row->addView(user);

        if (!post.caption.empty()) {
            auto* cap = new brls::Label();
            cap->setText(post.caption);
            cap->setFontSize(14.0f);
            cap->setTextColor(nvgRGB(0xC9, 0xCA, 0xD1));
            cap->setMarginTop(4.0f);
            row->addView(cap);
        }

        auto* meta = new brls::Label();
        //  = ícone "comment" do Material Icons (carregado como fallback);
        // o emoji 💬 (U+1F4AC) não existe nessa fonte e renderiza como caixa quebrada.
        meta->setText("♥ " + std::to_string(post.likeCount) +
                      "    " + std::to_string(post.commentCount));
        meta->setFontSize(13.0f);
        meta->setTextColor(nvgRGB(0x8b, 0x8d, 0x98));
        meta->setMarginTop(6.0f);
        row->addView(meta);

        row->registerClickAction([this, id](brls::View*) { this->showDetail(id); return true; });
        row->getFocusEvent()->subscribe([this, id](brls::View*) { this->showDetail(id); });
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));

        listBox->addView(row);

        if (i + 3 >= this->posts.size())
            this->loadNextPage();
    }
}

void FeedActivity::showDetail(const std::string& postId)
{
    if (this->selectedId == postId) return;
    this->selectedId = postId;

    auto* pane = (brls::Box*)this->getView("detailPane");
    if (!pane) return;
    pane->clearViews();

    feed::Post* post = feed::find_post(this->posts, postId);
    if (!post) return;

    // The detail content (image + caption + comments) is usually taller than the
    // pane, so it lives in a ScrollingFrame — otherwise yoga compresses the flex
    // children to fit and the header labels visually overlap.
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    auto* content = new brls::Box(brls::Axis::COLUMN);
    scroll->setContentView(content);
    pane->addView(scroll);

    auto* user = new brls::Label();
    user->setText("@" + post->author.username);
    user->setFontSize(18.0f);
    content->addView(user);

    if (post->gameTitleId != 0 && !post->gameName.empty()) {
        auto* game = new brls::Label();
        game->setText("thomaz/feed/game_tag"_i18n + post->gameName);
        game->setFontSize(13.0f);
        game->setTextColor(nvgRGB(0x92, 0x77, 0xFF));
        game->setMarginTop(6.0f);
        content->addView(game);
    }

    if (!post->imageUrl.empty()) {
        auto* img = new brls::Image();
        img->setHeight(300.0f);
        img->setMarginTop(14.0f);
        img->setCornerRadius(12.0f);
        img->setScalingType(brls::ImageScalingType::FIT);
        content->addView(img);

        std::string imageUrl = post->imageUrl;
        IFeedClient* ic = this->client; auto ialive = this->alive;
        brls::async([this, ic, ialive, imageUrl, postId, img]() {
            std::vector<std::uint8_t> bytes = ic->fetchImage(imageUrl);
            brls::sync([this, ialive, bytes, postId, img]() {
                if (!ialive->load()) return;
                // Só toca em `img` se este post ainda é o exibido: trocar de
                // detalhe chama clearViews() e libera `img` (use-after-free).
                if (this->selectedId != postId || bytes.empty()) return;
                img->setImageFromMem(bytes.data(), (int)bytes.size());
            });
        });
    }

    if (!post->caption.empty()) {
        auto* cap = new brls::Label();
        cap->setText(post->caption);
        cap->setFontSize(15.0f);
        cap->setMarginTop(14.0f);
        content->addView(cap);
    }

    auto* likeBtn = new brls::Box(brls::Axis::ROW);
    likeBtn->setFocusable(true);
    likeBtn->setHeight(40.0f);
    likeBtn->setMarginTop(14.0f);
    likeBtn->setPadding(6.0f, 14.0f, 6.0f, 14.0f);
    likeBtn->setCornerRadius(10.0f);
    likeBtn->setAlignItems(brls::AlignItems::CENTER);
    likeBtn->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));
    auto* likeLbl = new brls::Label();
    likeLbl->setText((post->likedByMe ? "♥ " : "♡ ") + std::to_string(post->likeCount));
    likeLbl->setFontSize(15.0f);
    likeBtn->addView(likeLbl);
    likeBtn->registerClickAction([this, postId, likeLbl](brls::View*) {
        if (!this->requireSession()) return true;
        feed::Post* p = feed::find_post(this->posts, postId);
        if (!p) return true;
        bool target = !p->likedByMe;
        p->likedByMe = target; p->likeCount += target ? 1 : -1;
        likeLbl->setText((target ? "♥ " : "♡ ") + std::to_string(p->likeCount));

        auto sess = load_session();
        std::string token = sess ? sess->token : "";
        IFeedClient* c = this->client; auto alive = this->alive;
        brls::async([this, c, alive, token, postId, target, likeLbl]() {
            ActionResult r = c->setLike(token, postId, target);
            brls::sync([this, alive, r, postId, target, likeLbl]() {
                if (!alive->load()) return;
                if (r.ok) return;
                // Revert the model first (always safe — only touches this->posts).
                feed::Post* p = feed::find_post(this->posts, postId);
                if (p) { p->likedByMe = !target; p->likeCount += target ? -1 : 1; }
                // Only touch likeLbl if this post is still the one on screen: a
                // detail switch calls clearViews() and frees likeLbl (use-after-free).
                if (p && this->selectedId == postId)
                    likeLbl->setText((p->likedByMe ? "♥ " : "♡ ") + std::to_string(p->likeCount));
            });
        });
        return true;
    });
    likeBtn->addGestureRecognizer(new brls::TapGestureRecognizer(likeBtn));
    content->addView(likeBtn);

    auto* commentsBox = new brls::Box(brls::Axis::COLUMN);
    commentsBox->setMarginTop(16.0f);
    content->addView(commentsBox);

    IFeedClient* c = this->client; auto alive = this->alive;
    brls::async([this, c, alive, postId, commentsBox]() {
        auto list = c->fetchComments(postId);
        brls::sync([this, alive, list, commentsBox, postId]() {
            if (!alive->load()) return;
            if (this->selectedId != postId) return;
            commentsBox->clearViews();
            for (const auto& cm : list) {
                auto* l = new brls::Label();
                l->setText("@" + cm.author.username + ": " + cm.text);
                l->setFontSize(13.0f);
                l->setMarginBottom(4.0f);
                commentsBox->addView(l);
            }
        });
    });

    auto* addBtn = new brls::Box(brls::Axis::ROW);
    addBtn->setFocusable(true);
    addBtn->setHeight(40.0f);
    addBtn->setMarginTop(10.0f);
    addBtn->setPadding(6.0f, 14.0f, 6.0f, 14.0f);
    addBtn->setCornerRadius(10.0f);
    addBtn->setAlignItems(brls::AlignItems::CENTER);
    addBtn->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));
    auto* addLbl = new brls::Label(); addLbl->setText("thomaz/feed/add_comment"_i18n);
    addLbl->setFontSize(14.0f); addBtn->addView(addLbl);
    addBtn->registerClickAction([this, postId, commentsBox](brls::View*) {
        if (!this->requireSession()) return true;
        brls::Application::getImeManager()->openForText(
            [this, postId, commentsBox](std::string text) {
                if (text.empty()) return;
                auto sess = load_session();
                std::string token = sess ? sess->token : "";
                IFeedClient* c = this->client; auto alive = this->alive;
                brls::async([this, c, alive, token, postId, text, commentsBox]() {
                    ActionResult r = c->addComment(token, postId, text);
                    brls::sync([this, alive, r, postId, commentsBox]() {
                        if (!alive->load()) return;
                        if (r.ok && this->selectedId == postId) this->showDetail(postId);
                    });
                });
            },
            "thomaz/feed/add_comment"_i18n, "", 280);
        return true;
    });
    addBtn->addGestureRecognizer(new brls::TapGestureRecognizer(addBtn));
    content->addView(addBtn);
}

bool FeedActivity::requireSession()
{
    if (load_session().has_value())
        return true;
    brls::Application::pushActivity(new AuthActivity(this->client, []() {}));
    return false;
}

void FeedActivity::onComposePressed()
{
    if (!this->requireSession()) return;
    auto alive = this->alive;
    brls::Application::pushActivity(new ComposerActivity(
        this->client, this->album, this->titles,
        [this, alive]() { if (alive->load()) this->loadFirstPage(); }));
}

} // namespace thomaz
