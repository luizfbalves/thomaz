#include "app/app_header.hpp"
#include <borealis.hpp>
#include "platform/feed/auth_store.hpp"

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

} // namespace thomaz
