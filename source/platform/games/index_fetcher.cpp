#include "platform/games/index_fetcher.hpp"

#include "core/games/recurse_plan.hpp"
#include "platform/games/index_fetch_util.hpp"

#include <borealis.hpp>
#include <deque>
#include <optional>
#include <set>
#include <utility>

namespace thomaz {

namespace {

constexpr std::size_t kMaxIndexBytes = 8u << 20;

std::optional<std::string> url_host_internal(const std::string& url) {
    const auto host = redacted_host_from_url(url);
    if (host == "(invalid-url)")
        return std::nullopt;
    return host;
}

std::optional<std::string> find_header(const HttpResponse& resp, const std::string& name) {
    std::string want;
    want.reserve(name.size());
    for (char c : name)
        want.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    for (const auto& h : resp.headers) {
        if (h.first == want)
            return h.second;
    }
    return std::nullopt;
}

std::string resolve_url(const std::string& base, const std::string& location) {
    if (location.find("://") != std::string::npos)
        return location;
    if (!location.empty() && location.front() == '/') {
        const auto scheme = base.find("://");
        if (scheme == std::string::npos)
            return location;
        const std::size_t hostStart = scheme + 3;
        const std::size_t pathStart = base.find('/', hostStart);
        if (pathStart == std::string::npos)
            return base + location;
        return base.substr(0, pathStart) + location;
    }
    const auto slash = base.rfind('/');
    if (slash == std::string::npos)
        return location;
    return base.substr(0, slash + 1) + location;
}

std::string apply_basic_in_url(const std::string& url, const std::string& secret) {
    if (secret.empty())
        return url;
    const auto scheme = url.find("://");
    if (scheme == std::string::npos)
        return url;
    const std::size_t hostStart = scheme + 3;
    if (url.find('@', hostStart) != std::string::npos)
        return url;
    return url.substr(0, hostStart) + secret + "@" + url.substr(hostStart);
}

void append_auth_headers(HttpRequest& req, const thomaz::core::SourceConfig& cfg,
                         bool attachCustomHeader) {
    switch (cfg.authType) {
    case thomaz::core::SourceAuthType::None:
    case thomaz::core::SourceAuthType::BasicInUrl:
        break;
    case thomaz::core::SourceAuthType::Header:
        if (attachCustomHeader && !cfg.authSecret.empty()) {
            const auto colon = cfg.authSecret.find(':');
            if (colon != std::string::npos) {
                std::string name  = cfg.authSecret.substr(0, colon);
                std::string value = cfg.authSecret.substr(colon + 1);
                while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
                    value.erase(value.begin());
                if (!name.empty())
                    req.headers.push_back({std::move(name), std::move(value)});
            }
        }
        break;
    case thomaz::core::SourceAuthType::Referrer:
        if (!cfg.authSecret.empty())
            req.headers.push_back({"Referer", cfg.authSecret});
        break;
    }
}

std::optional<std::string> fetch_index_body(IHttpClient*                      http,
                                            const thomaz::core::SourceConfig& cfg,
                                            const std::string&                url,
                                            const std::string&                originHost,
                                            std::shared_ptr<std::atomic<bool>> cancelled) {
    if (!http)
        return std::nullopt;

    std::string fetchUrl = url;
    if (cfg.authType == thomaz::core::SourceAuthType::BasicInUrl)
        fetchUrl = apply_basic_in_url(url, cfg.authSecret);

    HttpRequest req;
    req.method       = HttpMethod::Get;
    req.cancelled    = cancelled;
    req.maxBodyBytes = kMaxIndexBytes;

    if (cfg.authType == thomaz::core::SourceAuthType::Header) {
        req.followRedirects = false;
        std::string current = fetchUrl;
        for (int hop = 0; hop < 10; ++hop) {
            req.url = current;
            req.headers.clear();
            const auto currentHost = url_host_internal(current).value_or("");
            append_auth_headers(req, cfg, same_host(originHost, currentHost));
            const HttpResponse resp = http->request(req);
            if (resp.status >= 300 && resp.status < 400) {
                const auto loc = find_header(resp, "location");
                if (!loc)
                    return std::nullopt;
                current = resolve_url(current, *loc);
                continue;
            }
            if (!resp.ok() || resp.body.size() > kMaxIndexBytes)
                return std::nullopt;
            return resp.body;
        }
        return std::nullopt;
    }

    req.url = fetchUrl;
    append_auth_headers(req, cfg, true);
    const HttpResponse resp = http->request(req);
    if (!resp.ok() || resp.body.size() > kMaxIndexBytes)
        return std::nullopt;
    return resp.body;
}

void merge_index(thomaz::core::ParsedIndex& merged, const thomaz::core::ParsedIndex& part) {
    merged.files.insert(merged.files.end(), part.files.begin(), part.files.end());
    if (merged.motd.empty() && !part.motd.empty())
        merged.motd = part.motd;
    if (!part.ok)
        merged.ok = false;
}

} // namespace

FetchedCatalog fetch_index(IHttpClient*                         http,
                           const thomaz::core::SourceConfig&    cfg,
                           std::shared_ptr<std::atomic<bool>>   cancelled) {
    FetchedCatalog out;
    if (!http) {
        out.error = "no http client";
        return out;
    }
    if (cfg.url.empty()) {
        out.error = "empty source url";
        return out;
    }

    const std::string originHost = url_host_internal(cfg.url).value_or("");
    brls::Logger::info("thomaz/index: fetching catalog from host {}",
                       redacted_host_from_url(cfg.url));

    thomaz::core::RecurseBounds bounds;
    thomaz::core::RecurseState  state;
    std::set<std::string>       visited;
    std::deque<std::string>     queue;
    queue.push_back(cfg.url);

    while (!queue.empty()) {
        if (cancelled && cancelled->load()) {
            out.error = "cancelled";
            return out;
        }
        if (state.requests >= bounds.maxRequests) {
            out.truncated = true;
            break;
        }

        const std::string dirUrl = std::move(queue.front());
        queue.pop_front();
        if (!visited.insert(dirUrl).second)
            continue;

        state.requests++;
        auto body = fetch_index_body(http, cfg, dirUrl, originHost, cancelled);
        if (!body) {
            if (dirUrl == cfg.url) {
                out.error = "index fetch failed for " + redacted_host_from_url(cfg.url);
                return out;
            }
            out.truncated = true;
            continue;
        }

        thomaz::core::ParsedIndex part = thomaz::core::parse_index(*body);
        if (!part.ok && dirUrl == cfg.url) {
            out.error = "index parse failed for " + redacted_host_from_url(cfg.url);
            return out;
        }

        merge_index(out.merged, part);
        state.entries = out.merged.files.size();
        if (state.entries >= bounds.maxEntries) {
            out.truncated = true;
            break;
        }

        for (const std::string& child : part.directories) {
            thomaz::core::RecurseState childState = state;
            childState.depth++;
            if (!thomaz::core::may_descend(childState, bounds)) {
                out.truncated = true;
                continue;
            }
            queue.push_back(child);
        }
    }

    out.merged.ok = true;
    out.ok        = true;
    return out;
}

} // namespace thomaz
