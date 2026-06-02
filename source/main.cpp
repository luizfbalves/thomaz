/*
    thomaz — Nintendo Switch homebrew cheat manager hub.
    Entry point: initializes Borealis and pushes the home activity.
*/

#ifdef __SWITCH__
#include <switch.h>
#endif

#include <borealis.hpp>

#include "home_activity.hpp"

using namespace brls::literals; // for ""_i18n

int main(int argc, char* argv[])
{
    brls::Logger::setLogLevel(brls::LogLevel::INFO);

    if (!brls::Application::init())
    {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    brls::Application::createWindow("thomaz/title"_i18n);

    // Quit the app with the + (START) button from any activity.
    brls::Application::setGlobalQuit(true);

    brls::Application::pushActivity(new HomeActivity());

    while (brls::Application::mainLoop())
        ;

    return EXIT_SUCCESS;
}
