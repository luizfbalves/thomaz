/*
    thomaz — Nintendo Switch homebrew cheat manager hub.
    Entry point: initializes Borealis, registers the theme, selects the
    title service for the platform, and pushes the home activity.
*/

#ifdef __SWITCH__
#include <switch.h>
#include "platform/title_service_switch.hpp"
#include "platform/save_service_switch.hpp"
#else
#include "platform/title_service_fake.hpp"
#include "platform/save_service_fake.hpp"
#endif

#include <borealis.hpp>
#include <string>
#include "app/theme.hpp"
#include "app/animated_box.hpp"
#include "app/home_activity.hpp"
#include "app/boot_activity.hpp"
#include "platform/app_settings.hpp"
#include "platform/http_client_curl.hpp"
#include "platform/self_update.hpp"
#include "platform/http_auth_client.hpp"
#include "platform/feed/auth_store.hpp"
#include "platform/saves/http_cloud_save_client.hpp"

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

    // Smooth transitions: screens cross-fade/slide in on push, and the focus
    // ring glides between tiles instead of snapping. These drive view-owned
    // Animatables (View::alpha, highlightAlpha), so they are lifetime-safe.
    brls::Application::getStyle().addMetric("brls/animations/show", 220.0f);
    brls::Application::getStyle().addMetric("brls/animations/highlight", 140.0f);

    // Custom views used by the activity layouts.
    brls::Application::registerXMLView("AnimatedBox", thomaz::AnimatedBox::create);

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

    // Save backup/restore service for the current platform.
#ifdef __SWITCH__
    auto saveService = std::make_unique<thomaz::NsSaveService>();
#else
    auto saveService = std::make_unique<thomaz::FakeSaveService>();
#endif

    // HTTP client for downloading cheats (libcurl; libnx sockets on Switch).
    auto httpClient = std::make_unique<thomaz::CurlHttpClient>();

    // Auth client against the thomaz-api. Base URL has a compiled default
    // (localhost on desktop, prod host on Switch) plus a Settings override. The
    // client persists the session via auth_store on login; Cloud Saves reads the
    // access token from auth_store per call.
    std::string apiBaseUrl = thomaz::load_api_base_url();
    auto restoredSession   = thomaz::load_session();
    auto feedClient = std::make_unique<thomaz::HttpAuthClient>(
        httpClient.get(),
        apiBaseUrl,
        restoredSession,
        [](const thomaz::feed::Session& s) { thomaz::save_session(s); });

    // Cloud saves: real HTTP client against the same thomaz-api base URL. The
    // access token is read from auth_store per call by the Save Detail screen.
    auto cloudSaveClient = std::make_unique<thomaz::HttpCloudSaveClient>(
        httpClient.get(), apiBaseUrl);

    // Quit the app with the + (START) button from any activity.
    brls::Application::setGlobalQuit(true);

    if (restoredSession.has_value()) {
        // Session already restored — skip boot screen, go directly to home.
        brls::Application::pushActivity(
            new thomaz::HomeActivity(titleService.get(), httpClient.get(), saveService.get(),
                                     feedClient.get(), cloudSaveClient.get()));
    } else {
        // No saved session — show boot screen (login or guest).
        brls::Application::pushActivity(
            new thomaz::BootActivity(titleService.get(), httpClient.get(), saveService.get(),
                                     feedClient.get(), cloudSaveClient.get()));
    }

    while (brls::Application::mainLoop())
        ;

#ifdef __SWITCH__
    titleService->exit();
#endif

    return EXIT_SUCCESS;
}
