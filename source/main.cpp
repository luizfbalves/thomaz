/*
    thomaz — Nintendo Switch homebrew cheat manager hub.
    Entry point: initializes Borealis, registers the theme, selects the
    title service for the platform, and pushes the home activity.
*/

#ifdef __SWITCH__
#include <switch.h>
#include "platform/title_service_switch.hpp"
#else
#include "platform/title_service_fake.hpp"
#endif

#include <borealis.hpp>
#include <string>
#include "app/theme.hpp"
#include "app/home_activity.hpp"
#include "platform/app_settings.hpp"
#include "platform/http_client_curl.hpp"
#include "platform/self_update.hpp"

using namespace brls::literals; // for ""_i18n

int main(int argc, char* argv[])
{
    // Remember our launch path so the updater can replace the right .nro.
    thomaz::set_self_path(argc > 0 ? argv[0] : nullptr);

    brls::Logger::setLogLevel(brls::LogLevel::LOG_INFO);

    // UI language: use the user's saved choice, or follow the console language.
    std::string savedLocale = thomaz::load_locale();
    brls::Platform::APP_LOCALE_DEFAULT =
        (savedLocale == "auto") ? brls::LOCALE_AUTO : savedLocale;

    if (!brls::Application::init())
    {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    // Create the window/video context BEFORE touching the theme. On Switch with
    // deko3d, registerThomazTheme() calls setThemeVariant(), which immediately
    // does videoContext->recordStaticCommands() — but the videoContext only
    // exists after createWindow(). Registering the theme first dereferences a
    // null videoContext → Data Abort (null deref) on hardware. (Desktop's
    // setThemeVariant is a different impl, so this only crashed on the console.)
    brls::Application::createWindow("thomaz/title"_i18n);

    // Register theme colors and force dark mode (safe now: videoContext exists).
    thomaz::registerThomazTheme();

    // No animated transitions — screens swap instantly and focus snaps. A 0ms
    // "show" duration makes Application::pushActivity call show() with animate
    // off (it gates on duration > 0), so there is no cross-fade or slide.
    brls::Application::getStyle().addMetric("brls/animations/show", 0.0f);
    brls::Application::getStyle().addMetric("brls/animations/highlight", 0.0f);

    // Select the title service for the current platform.
#ifdef __SWITCH__
    auto titleService = std::make_unique<thomaz::NsTitleService>();
    if (!titleService->init())
    {
        brls::Logger::error("thomaz: failed to initialize ns title service");
        return EXIT_FAILURE;
    }
#else
    auto titleService = std::make_unique<thomaz::FakeTitleService>();
#endif

    // HTTP client for downloading cheats (libcurl; libnx sockets on Switch).
    auto httpClient = std::make_unique<thomaz::CurlHttpClient>();

    // Quit the app with the + (START) button from any activity.
    brls::Application::setGlobalQuit(true);

    brls::Application::pushActivity(new thomaz::HomeActivity(titleService.get(), httpClient.get()));

    while (brls::Application::mainLoop())
        ;

#ifdef __SWITCH__
    titleService->exit();
#endif

    return EXIT_SUCCESS;
}
