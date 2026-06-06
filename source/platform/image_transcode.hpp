#pragma once

#include <string>

namespace thomaz::platform {

// Theme images from themezer's CDN (img.themezer.net) are served as WebP, which
// the borealis image decoder (stb_image, via Image::setImageFromMem) cannot
// read — every decode fails with "unknown image type" and the preview renders
// blank (debug session: theme-preview-blank).
//
// If `bytes` is a WebP image, decode it and re-encode to PNG (which stb_image
// understands) so the existing setImageFromMem path keeps working untouched.
// For any non-WebP input — or on any decode/encode failure — the original bytes
// are returned unchanged, so the caller's normal path (and its diagnostics)
// still applies. Safe to call on every fetched image body.
std::string to_decodable_image(std::string bytes);

} // namespace thomaz::platform
