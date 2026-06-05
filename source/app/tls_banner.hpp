#pragma once

namespace brls { class Activity; }

namespace thomaz {

// Injects a persistent red warning Label into the AppletFrame header when
// thomaz::tls_is_insecure() is true (CA bundle probe failed on Switch).
// No-op on desktop (the latch is never set there) and when the header slot
// is not found. Call from onContentAvailable alongside install_header_username.
void install_tls_warning_banner(brls::Activity* activity);

} // namespace thomaz
