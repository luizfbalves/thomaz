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
#include "app/theme.hpp"
#include "app/home_activity.hpp"
#include "platform/http_client_curl.hpp"

using namespace brls::literals; // for ""_i18n

int main(int argc, char* argv[])
{
    brls::Logger::setLogLevel(brls::LogLevel::LOG_INFO);

    // Pick the UI language from the console's system language (pt-BR / en-US).
    brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;

    if (!brls::Application::init())
    {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    // Register theme colors and force dark mode.
    thomaz::registerThomazTheme();

    brls::Application::createWindow("thomaz/title"_i18n);

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
