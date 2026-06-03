#include "core/feed/feed_json.hpp"
#include <nlohmann/json.hpp>
#include <type_traits>

namespace thomaz::feed {

using nlohmann::json;

std::string build_credentials_body(const std::string& user, const std::string& pass)
{
    return json{ {"username", user}, {"password", pass} }.dump();
}

namespace {

// Non-throwing parse: returns a discarded json on any error.
json safe_parse(const std::string& body)
{
    return json::parse(body, nullptr, /*allow_exceptions=*/false);
}

// Type-checked scalar read: nlohmann's value(key, def) only falls back when the
// key is MISSING; a present-but-wrong-type value throws type_error.302. This
// returns the default for both the missing AND the wrong-type case.
template <class T>
T get_or(const json& j, const char* key, T def)
{
    if (!j.is_object() || !j.contains(key)) return def;
    const json& v = j.at(key);
    if constexpr (std::is_same_v<T, std::string>) {
        if (v.is_string()) return v.get<std::string>();
    } else if constexpr (std::is_same_v<T, bool>) {
        if (v.is_boolean()) return v.get<bool>();
    } else { // integral (int / std::int64_t)
        if (v.is_number_integer()) return v.get<T>();
    }
    return def;
}

// Maps a failed/non-2xx auth response to a stable, caller-displayable message.
// The activities show AuthResult.error verbatim when non-empty.
std::string auth_error_message(const json& j, long status)
{
    if (status == 0)   return "Sem conexão com o servidor.";
    if (status == 401) return "Usuário ou senha inválidos.";
    if (status == 409) return "Esse nome de usuário já existe.";
    if (status == 400) return "Verifique o usuário (3–32, letras/números/_) e a senha (mín. 6).";
    if (j.is_object() && j.contains("error") && j.at("error").is_string())
        return j.at("error").get<std::string>();
    return "Falha ao autenticar. Tente novamente.";
}

} // namespace

AuthResult parse_auth_response(const std::string& body, long status)
{
    AuthResult r;
    json j = safe_parse(body);
    const bool twoxx = (status >= 200 && status < 300);
    if (twoxx && !j.is_discarded() && get_or(j, "ok", false)) {
        r.ok           = true;
        r.token        = get_or(j, "token", std::string{});
        r.refreshToken = get_or(j, "refreshToken", std::string{});
        if (r.token.empty()) { r.ok = false; r.error = auth_error_message(j, status); }
        return r;
    }
    r.ok = false;
    r.error = auth_error_message(j.is_discarded() ? json::object() : j, status);
    return r;
}

} // namespace thomaz::feed
