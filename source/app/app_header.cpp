#include "app/app_header.hpp"
#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/views/button.hpp>
#include "app/auth_activity.hpp"
#include "platform/auth_client.hpp"
#include "platform/feed/auth_store.hpp"
#include "platform/system_status.hpp"
#include "core/storage_format.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {
// Shared auth client (owned by main) used to open the login screen from the
// header button on any activity.
IAuthClient* g_header_auth_client = nullptr;

// Update the button's label/color to reflect the current session state.
void refresh_auth_button(brls::Button* btn) {
    if (!btn) return;
    if (auto sess = load_session()) {
        btn->setText("@" + sess->username);
        btn->setTextColor(nvgRGB(0x92, 0x77, 0xFF));            // accent = signed in
    } else {
        btn->setText("thomaz/auth/login_tab"_i18n);
        btn->setTextColor(nvgRGB(0xF3, 0xF4, 0xF7));            // text = signed out
    }
}
} // namespace

void set_header_auth_client(IAuthClient* client) {
    g_header_auth_client = client;
}

void install_header_username(brls::Activity* activity)
{
    if (!activity) return;

    // O hint_box é o slot vazio à direita do header do AppletFrame (alinhado
    // pelo justifyContent="spaceBetween"). É o lugar natural para o status.
    auto* hintBox = dynamic_cast<brls::Box*>(activity->getView("brls/applet_frame/hint_box"));
    if (!hintBox) return;
    // Vertically center the login button with the storage indicators next to it.
    hintBox->setAlignItems(brls::AlignItems::CENTER);

    // Idempotent: if the button already exists (e.g. refreshHeaderUsername after a
    // boot-screen login), just refresh its text instead of adding a second one.
    if (auto* existing = dynamic_cast<brls::Button*>(activity->getView("headerAuthButton"))) {
        refresh_auth_button(existing);
        return;
    }

    // A real Button (bordered) showing the login status; clicking logs in/out.
    auto* btn = new brls::Button();
    btn->setId("headerAuthButton");
    btn->setStyle(&brls::BUTTONSTYLE_BORDERED);
    btn->setFontSize(15.0f);
    btn->setMarginLeft(8.0f);
    refresh_auth_button(btn);

    auto onClick = [btn](brls::View*) -> bool {
        if (load_session()) {
            // Signed in -> confirm, then log out and refresh the button in place.
            auto* dialog = new brls::Dialog("thomaz/auth/logout_confirm"_i18n);
            dialog->addButton("thomaz/auth/logout"_i18n, [btn]() {
                clear_session();
                refresh_auth_button(btn);
            });
            dialog->addButton("thomaz/auth/cancel"_i18n, []() {});
            dialog->open();
        } else if (g_header_auth_client) {
            // Signed out -> open login; AuthActivity pops itself on success and
            // runs this callback on the revealed (still-alive) activity.
            brls::Application::pushActivity(
                new AuthActivity(g_header_auth_client, [btn]() { refresh_auth_button(btn); }));
        }
        return true;
    };
    btn->registerClickAction(onClick);
    btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn));

    hintBox->addView(btn);
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
