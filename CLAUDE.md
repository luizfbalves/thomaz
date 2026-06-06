# thomaz — project conventions

## Running / verifying the app (ALWAYS use nxlink)

To run or verify any change in the real app, **always** use the netload + nxlink flow
on real hardware (Docker is broken on this machine; emulators don't cover the libnx
networking/theme paths). Never claim a change works without seeing it on hardware via
this loop.

1. **Build with nxlink stdio enabled** (devkitPro MSYS2 login shell — see the
   `switch-build-native` / `switch-nxlink-debug` memories). Always clean-build; the
   `compiler_depend.make` "múltiplos padrões" error breaks any incremental build:
   ```bash
   REPO=/c/www/thomaz; rm -rf $REPO/build_switch
   MSYSTEM=MSYS /c/devkitPro/msys2/usr/bin/bash.exe -lc \
     "cp -rn $REPO/lib/borealis/resources/* $REPO/resources/ 2>/dev/null; \
      cmake -S $REPO -B $REPO/build_switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON \
            -DUSE_DEKO3D=ON -DTHOMAZ_NXLINK=ON -DCMAKE_DEPENDS_USE_COMPILER=OFF && \
      make -C $REPO/build_switch thomaz.nro -j\$(nproc)"
   ```
   `-DTHOMAZ_NXLINK=ON` defines `DEBUG` for `switch_wrapper.c` so borealis calls
   `nxlinkStdio()` and `brls::Logger` output streams over the network.

2. **Netload + serve stdout** (run in the background; read the output file for live logs):
   ```bash
   /c/devkitPro/tools/bin/nxlink.exe -s /c/www/thomaz/build_switch/thomaz.nro
   ```
   The Switch must be at the Homebrew Menu (netloader listening) on the same LAN. Kill a
   stale server with `taskkill //F //IM nxlink.exe` before re-netloading; the user must
   return to hbmenu between runs.

3. **Drive the app on the Switch**, read the streamed `[INFO]/[ERROR]` lines from the
   nxlink output file, and confirm the change before declaring success.

See memories: `switch-build-native`, `switch-nxlink-debug`.
