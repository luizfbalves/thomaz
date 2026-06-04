#pragma once

namespace thomaz {

// Reboot the console back into CFW via reboot_to_payload (Switch only).
// Returns false if rebooting is unsupported or failed (desktop, or no payload
// registered) — the caller then tells the user to reboot manually. On success
// this does not return (the system reboots).
bool system_reboot_to_payload();

} // namespace thomaz
