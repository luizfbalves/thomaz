#include "platform/saves/http_cloud_save_client.hpp"
#include "core/saves/cloud_save_json.hpp"
#include <cstdio>
#include <utility>

namespace thomaz {

namespace {
std::string titleIdHex(std::uint64_t id) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)id);
    return std::string(buf);
}
} // namespace

HttpCloudSaveClient::HttpCloudSaveClient(IHttpClient* http, std::string baseUrl)
    : http(http), baseUrl(std::move(baseUrl)) {}

std::string HttpCloudSaveClient::savesUrl(std::uint64_t titleId) const {
    return baseUrl + "/saves/" + titleIdHex(titleId);
}

CloudStatus HttpCloudSaveClient::getStatus(const std::string& token, std::uint64_t titleId) {
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url    = savesUrl(titleId);
    req.headers.push_back({ "Authorization", "Bearer " + token });

    HttpResponse resp = http->request(req);
    CloudStatus s;
    if (resp.status == 401) { s.error = kCloudAuthExpired; return s; }

    auto meta = core::parse_slot_meta(resp.body, resp.status);
    if (!meta) { s.error = core::parse_error_message(resp.body, resp.status); return s; }

    s.ok        = true;
    s.exists    = meta->exists;
    s.revision  = meta->revision;
    s.label     = meta->label;
    s.updatedAt = meta->updatedAt;
    return s;
}

CloudPull HttpCloudSaveClient::pull(const std::string& token, std::uint64_t titleId) {
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url    = savesUrl(titleId) + "?includeData=1";
    req.headers.push_back({ "Authorization", "Bearer " + token });

    HttpResponse resp = http->request(req);
    CloudPull p;
    if (resp.status == 401) { p.error = kCloudAuthExpired; return p; }

    auto data = core::parse_slot_data(resp.body, resp.status);
    if (!data) { p.error = core::parse_error_message(resp.body, resp.status); return p; }

    p.ok        = true;
    p.exists    = data->meta.exists;
    p.revision  = data->meta.revision;
    p.label     = data->meta.label;
    p.updatedAt = data->meta.updatedAt;
    p.blob      = std::move(data->data);
    return p;
}

CloudPush HttpCloudSaveClient::push(const std::string& token, std::uint64_t titleId,
                                    const std::vector<std::uint8_t>& blob,
                                    const std::string& label, int revision) {
    HttpRequest req;
    req.method = HttpMethod::Put;
    req.url    = savesUrl(titleId);
    req.headers.push_back({ "Authorization", "Bearer " + token });
    req.fields.push_back({ "label", label });
    req.fields.push_back({ "revision", std::to_string(revision) });
    req.files.push_back(MultipartFile{ "data", titleIdHex(titleId) + ".bin",
                                       "application/octet-stream", blob });

    HttpResponse resp = http->request(req);
    CloudPush r;
    if (resp.status == 401) { r.error = kCloudAuthExpired; return r; }
    if (resp.status == 409) { r.conflict = true; return r; }
    if (!resp.ok()) { r.error = core::parse_error_message(resp.body, resp.status); return r; }

    auto rev = core::parse_push_revision(resp.body);
    if (!rev) { r.error = core::parse_error_message(resp.body, resp.status); return r; }
    r.ok          = true;
    r.newRevision = *rev;
    return r;
}

} // namespace thomaz
