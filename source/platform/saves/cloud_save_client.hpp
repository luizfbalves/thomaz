#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
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

// Cooperative-abort alias used by the cloud save client methods.
using CancelFlag = std::shared_ptr<std::atomic<bool>>;

// Talks to the thomaz-api /saves endpoints. All methods run on a brls::async
// worker thread and must not touch the UI. The token is supplied per call by
// the UI (read from auth_store).
//
// `cancelled` (optional, default null): cooperative abort flag.  Pass the
// owning activity's base-class cancelled shared_ptr; the underlying HTTP
// transport's XFERINFOFUNCTION will abort the transfer as soon as the flag is
// set.  Existing callers that omit the parameter are unaffected.
class ICloudSaveClient {
  public:
    virtual ~ICloudSaveClient() = default;

    virtual CloudStatus getStatus(const std::string& token, std::uint64_t titleId,
                                  CancelFlag cancelled = nullptr) = 0;
    virtual CloudPull   pull(const std::string& token, std::uint64_t titleId,
                             CancelFlag cancelled = nullptr) = 0;
    virtual CloudPush   push(const std::string& token, std::uint64_t titleId,
                             const std::vector<std::uint8_t>& blob,
                             const std::string& label, int revision,
                             CancelFlag cancelled = nullptr) = 0;
};

} // namespace thomaz
