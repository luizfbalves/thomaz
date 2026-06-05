#pragma once

namespace thomaz {

// Install the bundled qlaunch (Home Menu) exefs IPS patches into the CFW.
//
// On firmware 20.0+ a custom theme whose decompressed SZS is larger than the
// stock layout (which happens as soon as the background texture is swapped)
// crashes the Home Menu on boot (error 2168-0002) because qlaunch's
// SceneEntrance memory budget is exceeded. These IPS patches enlarge that
// budget. Each file is named after a qlaunch main-NSO build-id; Atmosphère
// applies only the one matching the running console, the rest are inert — so
// installing all of them is safe and firmware-agnostic.
//
// Idempotent (overwrites). Returns the number of .ips files written, or 0 if
// the bundle is missing / the CFW dir could not be created.
int install_qlaunch_patches();

} // namespace thomaz
