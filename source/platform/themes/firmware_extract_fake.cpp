#include "platform/themes/firmware_extract.hpp"

#ifndef __SWITCH__

namespace thomaz {

ExtractAllResult extract_all_base_layouts() {
    return {false, "Firmware extraction is only available on Switch.", {}, {}};
}

} // namespace thomaz

#endif // !__SWITCH__
