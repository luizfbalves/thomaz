#include "platform/system_status.hpp"

// Switch-only implementation. CMake globs every source file, so on desktop
// builds this translation unit must compile to nothing (no libnx there).
#ifdef __SWITCH__

#include <switch.h>
#include <algorithm>

namespace thomaz {

SystemStatus query_system_status()
{
    SystemStatus s{};

    // SD card space (ns service is already initialized at startup by NsTitleService::init)
    s64 sdTotal = 0, sdFree = 0;
    if (R_SUCCEEDED(nsGetTotalSpaceSize(NcmStorageId_SdCard, &sdTotal)))
        s.sd.total_bytes = (std::uint64_t)sdTotal;
    if (R_SUCCEEDED(nsGetFreeSpaceSize(NcmStorageId_SdCard, &sdFree)))
        s.sd.free_bytes  = (std::uint64_t)sdFree;

    // NAND/BuiltInUser space
    s64 nandTotal = 0, nandFree = 0;
    if (R_SUCCEEDED(nsGetTotalSpaceSize(NcmStorageId_BuiltInUser, &nandTotal)))
        s.nand.total_bytes = (std::uint64_t)nandTotal;
    if (R_SUCCEEDED(nsGetFreeSpaceSize(NcmStorageId_BuiltInUser, &nandFree)))
        s.nand.free_bytes  = (std::uint64_t)nandFree;

    // WiFi via nifm — init/exit per-call (fast IPC; R_FAILED leaves fields at 0)
    if (R_SUCCEEDED(nifmInitialize(NifmServiceType_User))) {
        NifmInternetConnectionType type{};
        u32 strength = 0;
        NifmInternetConnectionStatus status{};
        if (R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &strength, &status))) {
            s.wifi_connected = (status == NifmInternetConnectionStatus_Connected);
            s.wifi_strength  = (int)std::min<u32>(strength, 3u);  // clamp 0..3
        }
        nifmExit();
    }

    return s;
}

} // namespace thomaz

#else

namespace thomaz {

// Desktop stub: return zeroed status — bars will render empty, WiFi unlit.
SystemStatus query_system_status()
{
    return SystemStatus{};
}

} // namespace thomaz

#endif // __SWITCH__
