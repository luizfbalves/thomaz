#include "platform/games/index_fetch_util.hpp"

#include <cctype>
#include <optional>

namespace thomaz {

namespace {

std::string to_lower_ascii(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::optional<std::string> url_host(const std::string& url) {
    const auto scheme = url.find("://");
    if (scheme == std::string::npos)
        return std::nullopt;
    std::size_t start = scheme + 3;
    std::size_t end   = url.find('/', start);
    if (end == std::string::npos)
        end = url.size();
    std::string hostport = url.substr(start, end - start);
    const auto at        = hostport.rfind('@');
    if (at != std::string::npos)
        hostport = hostport.substr(at + 1);
    if (!hostport.empty() && hostport.front() == '[') {
        const auto close = hostport.find(']');
        if (close != std::string::npos)
            return to_lower_ascii(hostport.substr(1, close - 1));
    }
    const auto colon = hostport.rfind(':');
    if (colon != std::string::npos && hostport.find(':') == colon)
        hostport = hostport.substr(0, colon);
    if (hostport.empty())
        return std::nullopt;
    return to_lower_ascii(std::move(hostport));
}

} // namespace

bool same_host(const std::string& originHost, const std::string& targetHost) {
    if (originHost.empty() || targetHost.empty())
        return false;
    return to_lower_ascii(originHost) == to_lower_ascii(targetHost);
}

std::string redacted_host_from_url(const std::string& url) {
    if (auto h = url_host(url))
        return *h;
    return "(invalid-url)";
}

} // namespace thomaz
