#include "app/tls_banner.hpp"
#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include "platform/curl_tls.hpp"

using namespace brls::literals;

namespace thomaz {

void install_tls_warning_banner(brls::Activity* activity)
{
    if (!activity) return;

    // Desktop builds never set the latch — this is a no-op there.
    if (!thomaz::tls_is_insecure()) return;

    // The hint_box is the right-hand slot in the AppletFrame header, laid out
    // by justifyContent="spaceBetween". Mirrors the install_header_username
    // approach for app-wide injection (D-02a).
    auto* hintBox = dynamic_cast<brls::Box*>(activity->getView("brls/applet_frame/hint_box"));
    if (!hintBox)
        hintBox = dynamic_cast<brls::Box*>(activity->getView("brls/applet_frame/header"));
    if (!hintBox) {
        // Pairing a fail-open path with a fail-silent warning would compound the
        // risk: log loudly so an insecure session is never left fully unwarned.
        brls::Logger::warning("tls banner: no hint_box/header slot on this activity; insecure mode unwarned");
        return;
    }

    auto* lbl = new brls::Label();
    lbl->setText("thomaz/tls/insecure_warning"_i18n);
    lbl->setFontSize(16.0f);
    // High-contrast red — distinct from the username purple 0x9277FF.
    lbl->setTextColor(nvgRGB(0xFF, 0x55, 0x55));
    // Insert at index 0 so the warning appears before the username label.
    hintBox->addView(lbl, 0);
}

} // namespace thomaz
