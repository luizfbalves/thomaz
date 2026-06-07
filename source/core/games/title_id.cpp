#include "core/games/title_id.hpp"

namespace thomaz::core {

std::uint64_t base_title_id(std::uint64_t id) {
    return id & 0xFFFFFFFFFFFFE000ULL;
}

TitleKind title_kind(std::uint64_t id) {
    const std::uint64_t low = id & 0x1FFFULL;
    if (low == 0x000)
        return TitleKind::Base;
    if (low == 0x800)
        return TitleKind::Update;
    if (low >= 0x1000)
        return TitleKind::Dlc;
    return TitleKind::Unknown;
}

std::uint64_t dlc_content_index(std::uint64_t id) {
    return (id & 0x1FFFULL) - 0x1000;
}

} // namespace thomaz::core
