# Vendored: hactool fork (NCA/RomFS in-memory extraction)

- Upstream: https://github.com/exelix11/SwitchThemeInjector
- Pinned commit: 2618b0c31e007d019757dc4095eca08b4a89e3f5
- Imported path: SwitchThemesNX/Libs/hactool
- License: GPL v2 — see `LICENSE`.
- This project is distributed under GPLv2 because it links this library.

This is the exelix11 fork of SciresM/hactool, not the upstream hactool. The fork
adds an in-memory extraction buffer and a filename filter callback
(`extraction_file_stream_cb`, `romfs_filter`) that allow RomFS files to be
captured in memory rather than written to disk. This is the mechanism required
by the keyless on-device extraction path (EXTRACT-04).

Do NOT substitute the SciresM upstream hactool — that variant writes to disk
and lacks the in-memory filter callbacks this project depends on.

## Files

- `include/hactool.h` — public C header (entry points, keyset types, action flags)
- `source/*.c`, `source/*.h` — ~49 C source and header files (NCA, RomFS, BKTR,
  XCI, PFS0, HFS0, IVFc, key handling, etc.)
- `Makefile` — upstream build (devkitPro/libnx; used as reference for port)
- `LICENSE` — GNU GPL v2

## Adaptations (edits to the vendored tree)

This vendored copy is committed as-is from the pinned commit. No source edits
were applied at vendor time. Per Assumption A2 (01-RESEARCH.md), if the fork
builds with `-Wall -Werror` under the current devkitA64 toolchain and new
warnings surface due to toolchain drift (the upstream Makefile uses
`-Wall -Werror`), `-Werror` is relaxed for this vendored CMake target ONLY via
`target_compile_options(hactool PRIVATE -Wno-error)`. The app's own warning
flags are not affected. Any such relaxation is added in `lib/hactool/CMakeLists.txt`
and noted here as: [Toolchain-Werror relaxation applied if needed at build time].

## Usage

This library is built as a CMake static target (`hactool`) under
`lib/hactool/CMakeLists.txt` and linked only when `PLATFORM_SWITCH=ON`.
The thomaz-facing entry point is confined to
`source/platform/themes/firmware_extract_switch.cpp` (via `#ifdef __SWITCH__`),
which drives `nca_process` with the in-memory action flags and filter callback.
No other thomaz source file should include hactool headers directly.
