# Vendored: SwitchThemeInjector C++ engine (apply subset)

- Upstream: https://github.com/exelix11/SwitchThemeInjector
- Pinned commit: 2618b0c31e007d019757dc4095eca08b4a89e3f5
- Imported path: SwitchThemesNX/source/SwitchThemesCommon/
- License: GPL v2 (see repo root LICENSE / our LICENSE).
- Removed: firmware extraction glue (hactool/mbedtls/key_loader/RomfsCache) and
  upstream fs.cpp — base layouts come from /themes/systemData via our cfw_paths;
  install paths via our theme_install. None of those .cpp files lived inside the
  imported SwitchThemesCommon/ tree, so nothing had to be dropped from the engine;
  the only edits are include/dependency rewrites (see Adaptations).
- Entry point used by the app: apply_facade.{hpp,cpp} (only). All upstream-API
  knowledge (NxTheme::TryLoad, SzsPatcher, SARC/Yaz0, Patches::LoadLayout) is
  confined to apply_facade.cpp, which mirrors NxEntry.cpp::DoInstall's apply path.
- json: using upstream's bundled json.hpp (nlohmann 3.9.1) kept at
  lib/switchthemes/json.hpp — engine includes resolve locally, no churn, no
  clash with the app's lib/json/nlohmann/json.hpp.
- images: third_party/stb_image.h (vanilla nothings/stb — upstream's was the
  SOIL2-extended variant that #includes stbi_DDS/pvr/pkm/qoi, none of which the
  apply path uses), plus stb_dxt.h + stb_image_resize2.h copied from upstream's
  Libs. The implementations are emitted once, with STATIC linkage, inside
  Bntx/DDS_conversion.cpp (its only consumer). STATIC is required so they do not
  collide at link time with borealis's own bundled stb_image (nanovg), which
  already exports stbi_load_from_memory / stbi_image_free / stbi_failure_reason.
- zip: third_party/miniz.{h,c} (amalgamated miniz 3.0.2 / MZ_VERSION 11.0.2).
- C++ standard: the engine needs C++20 (std::span, operator<=>, std::format), so
  the app target was bumped from C++17 to C++20 (backward compatible).

## Generated/ (committed, normally code-gen output)
Upstream produces three files via the C# NxThemeTool `cppgen` step; that tool was
unavailable in this environment (no .NET SDK), so they were reproduced by hand
from the same upstream data at the pinned commit, and committed (the upstream
.gitignore that excluded them was removed):
- Generated/NewFirmFixes.g.hpp — each compatibility fix as a minified-JSON
  std::string_view, taken verbatim from NxThemeTool/Compatibility/*.json (the
  same JSON the engine's Patches::LoadLayout parses at runtime).
- Generated/PatchTemplates.g.cpp — Patches::DefaultTemplates, transcribed from
  SwitchThemesCommon/Layouts/PatchTemplate.cs (DefaultTemplates.Templates).
- Generated/TextureReplacement.g.cpp — Patches::textureReplacement::NxNameToList,
  transcribed from the same file. Only needed to satisfy the linker
  (Patcher::PatchAppletIcon); the apply_facade path does not exercise it.

## Adaptations (only edits to the vendored tree)
- Common.cpp: dropped `#include "../fs.hpp"`; replaced the single `fs::GetFileName`
  use (in FindBySzsName) with a local basename helper.
- Bntx/DDS_conversion.cpp: dropped the unused `Platform/Platform.hpp` (GLFW glue)
  include; repointed the three stb includes at third_party/.
- NXTheme.cpp: replaced the kuba--/zip wrapper (`../../Libs/zip/zip.h`) with
  direct miniz calls in `zip::Extract`; added `<cstring>`.
