#pragma once

namespace thomaz {

// Reboot the console (reboot-to-payload via spsm) so LayeredFS theme changes
// take effect. No-op on desktop builds.
void reboot_to_payload();

} // namespace thomaz
