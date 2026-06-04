#include "app/app_header.hpp"
#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include "platform/feed/auth_store.hpp"

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
