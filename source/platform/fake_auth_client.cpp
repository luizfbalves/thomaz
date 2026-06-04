#include "platform/fake_auth_client.hpp"

namespace thomaz {

AuthResult FakeAuthClient::registerUser(const std::string& user, const std::string&)
{
    if (user.empty())
        return { false, "", "username obrigatório" };
    return { true, "fake-token-" + user, "" };
}

AuthResult FakeAuthClient::login(const std::string& user, const std::string&)
{
    if (user.empty())
        return { false, "", "username obrigatório" };
    return { true, "fake-token-" + user, "" };
}

} // namespace thomaz
