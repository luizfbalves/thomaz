#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz {

// Error sentinel set when the API returns 401. The UI maps this to a re-login
// prompt (we do not auto-refresh the token from the save client).
inline constexpr const char* kCloudAuthExpired = "unauthorized";

struct CloudStatus {
    bool         ok        = false; // request completed (exists may still be false)
    bool         exists    = false;
    int          revision  = 0;
    std::string  label;
    std::int64_t updatedAt = 0;
    std::string  error;             // set when !ok
};

struct CloudPull {
    bool                      ok        = false;
    bool                      exists    = false;
    int                       revision  = 0;
    std::string               label;
    std::int64_t              updatedAt = 0;
    std::vector<std::uint8_t> blob;
    std::string               error;
};

struct CloudPush {
    bool        ok          = false;
    bool        conflict    = false; // HTTP 409 (revision_conflict)
    int         newRevision = 0;
    std::string error;
};

// Talks to the thomaz-api /saves endpoints. All methods run on a brls::async
// worker thread and must not touch the UI. The token is supplied per call by
// the UI (read from auth_store).
class ICloudSaveClient {
  public:
    virtual ~ICloudSaveClient() = default;

    virtual CloudStatus getStatus(const std::string& token, std::uint64_t titleId) = 0;
    virtual CloudPull   pull(const std::string& token, std::uint64_t titleId) = 0;
    virtual CloudPush   push(const std::string& token, std::uint64_t titleId,
                             const std::vector<std::uint8_t>& blob,
                             const std::string& label, int revision) = 0;
};

} // namespace thomaz
