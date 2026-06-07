#include "platform/games/http_source_sync_client.hpp"

#include "core/games/source_link.hpp"
#include "core/saves/cloud_save_json.hpp"

#include <nlohmann/json.hpp>
#include <utility>

namespace thomaz {

namespace {

using nlohmann::json;

std::string build_put_body(const core::SourceConfig& cfg) {
    json j = json::parse(core::serialize_source_link(cfg), nullptr,
                         /*allow_exceptions=*/false);
    if (j.is_discarded())
        return {};
    if (!cfg.authSecret.empty())
        j["secret"] = cfg.authSecret;
    return j.dump();
}

} // namespace

HttpSourceSyncClient::HttpSourceSyncClient(IHttpClient* http, std::string baseUrl)
    : http(http), baseUrl(std::move(baseUrl)) {}

std::string HttpSourceSyncClient::sourceUrl(const std::string& id) const {
    return baseUrl + "/sources/" + id;
}

SourceSyncList HttpSourceSyncClient::list(const std::string& token, CancelFlag cancelled) {
    HttpRequest req;
    req.method    = HttpMethod::Get;
    req.url       = baseUrl + "/sources";
    req.cancelled = cancelled;
    req.headers.push_back({ "Authorization", "Bearer " + token });

    HttpResponse resp = http->request(req);
    SourceSyncList out;
    if (resp.status == 401) {
        out.error = kCloudAuthExpired;
        return out;
    }
    if (!resp.ok()) {
        out.error = core::parse_error_message(resp.body, resp.status);
        return out;
    }

    json root = json::parse(resp.body, nullptr, /*allow_exceptions=*/false);
    if (root.is_discarded() || !root.is_object() || !root["sources"].is_array()) {
        out.error = "invalid_response";
        return out;
    }

    for (const auto& item : root["sources"]) {
        if (!item.is_object())
            continue;
        auto cfg = core::parse_source_link(item.dump());
        if (!cfg)
            continue;
        out.sources.push_back(std::move(*cfg));
    }

    out.ok = true;
    return out;
}

SourceSyncResult HttpSourceSyncClient::push(const std::string& token, const std::string& id,
                                            const core::SourceConfig& cfg,
                                            CancelFlag cancelled) {
    HttpRequest req;
    req.method    = HttpMethod::Put;
    req.url       = sourceUrl(id);
    req.cancelled = cancelled;
    req.headers.push_back({ "Authorization", "Bearer " + token });
    req.headers.push_back({ "Content-Type", "application/json" });
    req.body      = build_put_body(cfg);

    HttpResponse resp = http->request(req);
    SourceSyncResult out;
    if (resp.status == 401) {
        out.error = kCloudAuthExpired;
        return out;
    }
    if (!resp.ok()) {
        out.error = core::parse_error_message(resp.body, resp.status);
        return out;
    }

    json root = json::parse(resp.body, nullptr, /*allow_exceptions=*/false);
    if (root.is_discarded() || !root.is_object() || !root["source"].is_object()) {
        out.error = "invalid_response";
        return out;
    }

    out.ok = true;
    out.id = root["source"].value("id", id);
    return out;
}

SourceSyncResult HttpSourceSyncClient::remove(const std::string& token, const std::string& id,
                                              CancelFlag cancelled) {
    HttpRequest req;
    req.method    = HttpMethod::Delete;
    req.url       = sourceUrl(id);
    req.cancelled = cancelled;
    req.headers.push_back({ "Authorization", "Bearer " + token });

    HttpResponse resp = http->request(req);
    SourceSyncResult out;
    if (resp.status == 401) {
        out.error = kCloudAuthExpired;
        return out;
    }
    if (!resp.ok()) {
        out.error = core::parse_error_message(resp.body, resp.status);
        return out;
    }

    out.ok = true;
    out.id = id;
    return out;
}

} // namespace thomaz
