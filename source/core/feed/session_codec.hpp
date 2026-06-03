#pragma once
#include <optional>
#include <string>
#include "core/feed/feed_types.hpp"

namespace thomaz::feed {

// Formato em disco: "<token>\n<username>\n". Funções puras (sem IO) para
// serem testáveis; o auth_store faz o read/write do arquivo.
std::string serialize_session(const Session& s);
std::optional<Session> parse_session(const std::string& text);

} // namespace thomaz::feed
