/*
    thomaz — game list activity implementation.
*/

#include "app/game_list_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>

using namespace brls::literals;

namespace thomaz {

GameListActivity::GameListActivity(ITitleService* titleService)
    : titleService(titleService)
{
}

void GameListActivity::onContentAvailable()
{
    brls::Box* listBox = (brls::Box*)this->getView("gameListBox");
    brls::Label* emptyLabel = (brls::Label*)this->getView("emptyLabel");

    auto titles = this->titleService->listInstalled();

    if (titles.empty())
    {
        // Show empty state, hide the list container.
        if (emptyLabel)
            emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        if (listBox)
            listBox->setVisibility(brls::Visibility::GONE);
        return;
    }

    // Hide empty label.
    if (emptyLabel)
        emptyLabel->setVisibility(brls::Visibility::GONE);

    if (!listBox)
        return;

    for (const auto& title : titles)
    {
        // Build a focusable row: a Box with a name label and a version label.
        brls::Box* row = new brls::Box(brls::Axis::ROW);
        row->setWidth(brls::View::AUTO);
        row->setHeight(64.0f);
        row->setFocusable(true);
        row->setMarginBottom(4.0f);
        row->setPadding(12.0f, 20.0f, 12.0f, 20.0f);
        row->setBackgroundColor(nvgRGB(0x1E, 0x20, 0x27));
        row->setCornerRadius(8.0f);

        // Game name (grows).
        brls::Label* nameLabel = new brls::Label();
        nameLabel->setWidth(brls::View::AUTO);
        nameLabel->setHeight(brls::View::AUTO);
        nameLabel->setGrow(1.0f);
        nameLabel->setText(title.name);
        nameLabel->setFontSize(18.0f);
        row->addView(nameLabel);

        // Version (formatted as decimal).
        brls::Label* versionLabel = new brls::Label();
        versionLabel->setWidth(brls::View::AUTO);
        versionLabel->setHeight(brls::View::AUTO);
        std::string versionStr = "v" + std::to_string(title.version);
        versionLabel->setText(versionStr);
        versionLabel->setFontSize(14.0f);
        row->addView(versionLabel);

        // Tapping shows a "coming soon" toast.
        row->registerClickAction([](brls::View* view) {
            brls::Application::notify("thomaz/games/coming_soon"_i18n);
            return true;
        });

        listBox->addView(row);
    }
}

} // namespace thomaz
