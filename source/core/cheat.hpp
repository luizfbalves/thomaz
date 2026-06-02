#pragma once
#include <string>
#include <vector>

namespace thomaz::core {

// One cheat parsed from an Atmosphère cheat .txt file.
struct Cheat {
    std::string name;                     // display name, WITHOUT brackets/braces
    bool is_master = false;               // true when the header used { } (anchor/master code)
    std::vector<std::string> opcode_lines; // opcode lines (no blanks), in original order

    bool operator==(const Cheat& o) const {
        return name == o.name && is_master == o.is_master && opcode_lines == o.opcode_lines;
    }
};

} // namespace thomaz::core
