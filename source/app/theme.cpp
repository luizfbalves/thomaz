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

    // Register custom colors into the dark theme (design "Signal" tokens).
    auto& dark = brls::Theme::getDarkTheme();

    // Surfaces (built around #14151A).
    dark.addColor("thomaz/bg",         nvgRGB(0x0E, 0x0F, 0x14)); // deepest backdrop
    dark.addColor("thomaz/surface_0",  nvgRGB(0x14, 0x15, 0x1A)); // frame base
    dark.addColor("thomaz/surface_1",  nvgRGB(0x1A, 0x1C, 0x23)); // cards / rows
    dark.addColor("thomaz/surface_2",  nvgRGB(0x22, 0x24, 0x2D)); // hover / focused row
    dark.addColor("thomaz/surface_3",  nvgRGB(0x2B, 0x2E, 0x39)); // switch track / chips
    dark.addColor("thomaz/surface",    nvgRGB(0x1A, 0x1C, 0x23)); // legacy alias

    // Hairlines.
    dark.addColor("thomaz/line",       nvgRGBA(0xFF, 0xFF, 0xFF, 0x12));
    dark.addColor("thomaz/line_2",     nvgRGBA(0xFF, 0xFF, 0xFF, 0x1F));

    // Text.
    dark.addColor("thomaz/text",       nvgRGB(0xF3, 0xF4, 0xF7));
    dark.addColor("thomaz/text_dim",   nvgRGB(0x9D, 0xA1, 0xAD));
    dark.addColor("thomaz/text_faint", nvgRGB(0x5C, 0x60, 0x6D));

    // Accent ramp (#7C5CFF).
    dark.addColor("thomaz/accent",        nvgRGB(0x7C, 0x5C, 0xFF));
    dark.addColor("thomaz/accent_bright", nvgRGB(0x92, 0x77, 0xFF));
    dark.addColor("thomaz/accent_deep",   nvgRGB(0x5E, 0x40, 0xE6));
    dark.addColor("thomaz/accent_soft",   nvgRGBA(0x7C, 0x5C, 0xFF, 0x29));
    dark.addColor("thomaz/accent_line",   nvgRGBA(0x7C, 0x5C, 0xFF, 0x73));
    dark.addColor("thomaz/good",          nvgRGB(0x57, 0xC9, 0x8A));

    // Borealis surfaces + focus ring → our palette.
    dark.addColor("brls/background",      nvgRGB(0x0E, 0x0F, 0x14));
    dark.addColor("brls/accent",          nvgRGB(0x7C, 0x5C, 0xFF));
}

} // namespace thomaz
