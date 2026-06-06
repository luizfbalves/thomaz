#include "app/app_header.hpp"
#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include "platform/feed/auth_store.hpp"
#include "platform/system_status.hpp"
#include "core/storage_format.hpp"

using namespace brls::literals;

namespace thomaz {

void install_header_username(brls::Activity* activity)
{
    if (!activity) return;

    // O hint_box é o slot vazio à direita do header do AppletFrame (alinhado
    // pelo justifyContent="spaceBetween"). É o lugar natural para o username.
    auto* hintBox = dynamic_cast<brls::Box*>(activity->getView("brls/applet_frame/hint_box"));
    if (!hintBox) return;

    auto sess = load_session();
    if (!sess) return;

    auto* lbl = new brls::Label();
    lbl->setText("@" + sess->username);
    lbl->setFontSize(16.0f);
    lbl->setTextColor(nvgRGB(0x92, 0x77, 0xFF));
    hintBox->addView(lbl);
}

void install_system_status(brls::Activity* activity)
{
    if (!activity) return;

    auto* hintBox = dynamic_cast<brls::Box*>(activity->getView("brls/applet_frame/hint_box"));
    if (!hintBox) return;

    auto status = query_system_status();

    // Outer row that holds all three indicators.
    auto* row = new brls::Box();
    row->setAxis(brls::Axis::ROW);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setMarginRight(8.0f);

    // Build one compact column per storage device (SD then NAND).
    auto makeStorageIndicator = [&](const std::string& labelStr, const StorageInfo& info) -> brls::Box* {
        auto* col = new brls::Box();
        col->setAxis(brls::Axis::COLUMN);
        col->setAlignItems(brls::AlignItems::FLEX_START);
        col->setMarginRight(10.0f);

        // Top label: "SD" or "Console"
        auto* lbl = new brls::Label();
        lbl->setText(labelStr);
        lbl->setFontSize(20.0f);
        lbl->setTextColor(nvgRGBA(0xFF, 0xFF, 0xFF, 0x99));  // dim white

        // Progress bar track
        static constexpr float kTrackW = 160.0f;
        static constexpr float kTrackH = 10.0f;
        auto* track = new brls::Box();
        track->setWidth(kTrackW);
        track->setHeight(kTrackH);
        track->setCornerRadius(5.0f);
        track->setBackgroundColor(nvgRGBA(0xFF, 0xFF, 0xFF, 0x33));  // dim surface

        // Fill bar
        float ratio = used_ratio(info.total_bytes, info.free_bytes);
        float fillW = kTrackW * ratio;
        if (fillW < 1.0f && ratio > 0.0f) fillW = 1.0f;  // at least 1px when non-zero
        auto* fill = new brls::Box();
        fill->setWidth(fillW);
        fill->setHeight(kTrackH);
        fill->setCornerRadius(5.0f);
        fill->setBackgroundColor(nvgRGB(0x92, 0x77, 0xFF));  // accent_bright (matches username color)

        track->addView(fill);
        col->addView(lbl);
        col->addView(track);
        return col;
    };

    row->addView(makeStorageIndicator("SD", status.sd));
    row->addView(makeStorageIndicator("Console", status.nand));

    // WiFi indicator removed — already shown in the AppletFrame footer.

    hintBox->addView(row);
}

void install_help_action(brls::Activity* activity, const char* frameId,
                         const std::string& bodyKey)
{
    if (!activity) return;

    auto* frame = activity->getView(frameId);
    if (!frame) return;

    std::string body = brls::getStr(bodyKey);

    // "-" (Minus) -> BUTTON_BACK. Hint aparece no rodapé; ao acionar, abre um
    // diálogo de ajuda com um único botão para fechar.
    frame->registerAction(
        "thomaz/help/hint"_i18n, brls::BUTTON_BACK,
        [body](brls::View*) {
            auto* dialog = new brls::Dialog(body);
            dialog->addButton("thomaz/help/close"_i18n, []() {});
            dialog->open();
            return true;
        },
        false);
}

} // namespace thomaz
