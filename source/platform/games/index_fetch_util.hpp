#pragma once

#include <string>

namespace thomaz {

// Pure helper — host unit-tested. Compares host parts case-insensitively.
bool same_host(const std::string& originHost, const std::string& targetHost);

std::string redacted_host_from_url(const std::string& url);

} // namespace thomaz
