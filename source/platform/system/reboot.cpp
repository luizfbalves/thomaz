#include "platform/system/reboot.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace thomaz {

void reboot_to_payload() {
#ifdef __SWITCH__
    // spsmShutdown(true) reboots; matches NXThemes Installer's PlatformReboot.
    spsmInitialize();
    spsmShutdown(true);
    spsmExit();
#endif
    // desktop: intentionally nothing.
}

} // namespace thomaz
