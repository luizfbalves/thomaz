/*
    thomaz — Nintendo Switch homebrew cheat manager hub.
    Entry point: initializes Borealis and pushes the home activity.
*/

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <borealis.hpp>

#include "home_activity.hpp"
#ifdef __SWITCH__
#include "platform/title_service_switch.hpp"
#endif

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

#ifdef __SWITCH__
    // Phase 3 sanity signal (replaced by the bento/list UI in Phase 4):
    // list installed games and log how many were found.
    {
        thomaz::NsTitleService titleService;
        if (titleService.init())
        {
            auto titles = titleService.listInstalled();
            brls::Logger::info("thomaz: found {} installed titles", titles.size());
            titleService.exit();
        }
        else
        {
            brls::Logger::error("thomaz: failed to initialize ns title service");
        }
    }
#endif

    brls::Application::createWindow("thomaz/title"_i18n);

    // Quit the app with the + (START) button from any activity.
    brls::Application::setGlobalQuit(true);

    brls::Application::pushActivity(new HomeActivity());

    while (brls::Application::mainLoop())
        ;

    return EXIT_SUCCESS;
}
