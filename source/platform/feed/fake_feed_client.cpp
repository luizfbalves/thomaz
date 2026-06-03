#include "platform/feed/fake_feed_client.hpp"

namespace thomaz {

AuthResult FakeFeedClient::registerUser(const std::string& user, const std::string&)
{
    if (user.empty())
        return { false, "", "username obrigatório" };
    return { true, "fake-token-" + user, "" };
}

AuthResult FakeFeedClient::login(const std::string& user, const std::string&)
{
    if (user.empty())
        return { false, "", "username obrigatório" };
    return { true, "fake-token-" + user, "" };
}

} // namespace thomaz
