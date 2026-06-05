#include "platform/themes/firmware_extract.hpp"

#ifndef __SWITCH__

namespace thomaz {

ExtractResult extract_base_layout(const std::string& /*target*/) {
    return {false, "Firmware extraction is only available on Switch."};
}

} // namespace thomaz

#endif // !__SWITCH__
