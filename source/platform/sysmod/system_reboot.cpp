#include "platform/sysmod/system_reboot.hpp"

#ifdef __SWITCH__

#include <switch.h>

namespace thomaz {

bool system_reboot_to_payload() {
    Result rc = spsmInitialize();
    if (R_FAILED(rc))
        return false;
    // Reboot into the registered payload (Hekate/fusee). On success this call
    // does not return.
    rc = spsmShutdown(true);
    spsmExit();
    return R_SUCCEEDED(rc);
}

} // namespace thomaz

#else

namespace thomaz {

bool system_reboot_to_payload() {
    return false; // unsupported on desktop
}

} // namespace thomaz

#endif // __SWITCH__
