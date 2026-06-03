#pragma once
#include <optional>
#include "core/feed/feed_types.hpp"

namespace thomaz {

// Sessão persistida (mantém login entre execuções). Arquivo na SD no Switch,
// pasta de trabalho no desktop — mesmo padrão de app_settings.
std::optional<feed::Session> load_session();
void save_session(const feed::Session& s);
void clear_session();

} // namespace thomaz
