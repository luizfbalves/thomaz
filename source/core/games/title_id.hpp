#pragma once

#include <cstdint>

namespace thomaz::core {

enum class TitleKind { Base, Update, Dlc, Unknown };

std::uint64_t base_title_id(std::uint64_t id);
TitleKind     title_kind(std::uint64_t id);
std::uint64_t dlc_content_index(std::uint64_t id);

} // namespace thomaz::core
