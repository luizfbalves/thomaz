/*
    thomaz — app theme registration.
*/

#include "app/theme.hpp"

#include <borealis.hpp>
#include <nanovg.h>

namespace thomaz {

void registerThomazTheme()
{
    // Force dark mode.
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);

    // Register custom colors into the dark theme.
    auto& dark = brls::Theme::getDarkTheme();
    dark.addColor("thomaz/bg",       nvgRGB(0x14, 0x15, 0x1A));
    dark.addColor("thomaz/surface",  nvgRGB(0x1E, 0x20, 0x27));
    dark.addColor("thomaz/accent",   nvgRGB(0x7C, 0x5C, 0xFF));
    dark.addColor("thomaz/text",     nvgRGB(0xFF, 0xFF, 0xFF));
    dark.addColor("thomaz/text_dim", nvgRGB(0x9A, 0xA0, 0xAA));

    // Override the borealis background with our dark bg.
    dark.addColor("brls/background", nvgRGB(0x14, 0x15, 0x1A));
}

} // namespace thomaz
