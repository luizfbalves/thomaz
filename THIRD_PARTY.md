# Third-Party Components

## SwitchThemeInjector (theme engine)
- Source: https://github.com/exelix11/SwitchThemeInjector
- Files: vendored under `lib/switchthemes/` (the C++ `SwitchThemesCommon` apply subset)
- License: GNU GPL v2 — see `LICENSE`.
- This project is distributed under GPLv2 because it links this engine.

## stb_image
- Source: https://github.com/nothings/stb (`stb_image.h`)
- File: `lib/switchthemes/third_party/stb_image.h`
- License: Public Domain / MIT (dual). See header.

## miniz
- Source: https://github.com/richgel999/miniz
- Files: `lib/switchthemes/third_party/miniz.{h,c}`
- License: MIT. See header.

## nlohmann/json
- Source: https://github.com/nlohmann/json
- File: `lib/json/nlohmann/json.hpp`
- License: MIT.

## hactool (NCA/RomFS extraction)
- Source: https://github.com/exelix11/SwitchThemeInjector (exelix11 fork of SciresM/hactool)
- Pinned commit: `2618b0c31e007d019757dc4095eca08b4a89e3f5`
- Imported path: `SwitchThemesNX/Libs/hactool`
- Files: vendored under `lib/hactool/` (include/, source/, Makefile, LICENSE)
- License: GNU GPL v2 — see `lib/hactool/LICENSE`.
- This project is distributed under GPLv2 because it links this fork.
- Note: this is the exelix11 fork with in-memory buffer + filter callbacks; NOT
  the SciresM upstream hactool (disk-write variant).

## mbedtls (CMAC build)
- Source: https://github.com/Mbed-TLS/mbedtls
- Pinned tag: `mbedtls-2.28.10`
- Files: vendored under `lib/mbedtls/` (include/, library/, 3rdparty/)
- License: Apache-2.0 OR GPL-2.0-or-later — see `lib/mbedtls/LICENSE`.
- Built from source with `MBEDTLS_CMAC_C` enabled (see `lib/mbedtls/thomaz_cmac_config.h`)
  as a distinct static target `thomaz_mbedtls_cmac` separate from the devkitPro portlib
  mbedtls that curl links against.
- No prebuilt `.a` blob committed (D-06 reproducibility).

## SPL key sources (Atmosphère)
- Source: https://github.com/Atmosphere-NX/Atmosphere (FS crypto config)
- Pinned release + firmware version: to be recorded after hardware validation spike
  (see TAKEOVER-02 doc; provisioned in plan 01-05 after first hardware run).
- These are PUBLIC key sources (header_kek_source, header_key_source,
  key_area_key_application_source) used to derive per-console keys on-device via
  SPL. They are never decrypted keys and do not constitute Nintendo IP.
