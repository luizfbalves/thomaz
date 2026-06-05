#include "core/title_filter.hpp"

namespace thomaz {
namespace core {

TitleKind classify(const InstalledTitle& t)
{
    // Heuristic: homebrew/forwarder has no user-account save and no account requirement.
    if (t.save_data_size == 0 && t.startup_user_account == 0)
        return TitleKind::NonGame;
    return TitleKind::Game;
}

bool effectively_hidden(const InstalledTitle& t,
                        const std::set<std::uint64_t>& force_hidden,
                        const std::set<std::uint64_t>& force_shown)
{
    if (force_shown.count(t.title_id))
        return false;
    if (force_hidden.count(t.title_id))
        return true;
    return classify(t) == TitleKind::NonGame;
}

} // namespace core
} // namespace thomaz
