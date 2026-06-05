#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "platform/saves/cloud_save_client.hpp"

namespace thomaz {

// In-memory ICloudSaveClient for tests and offline desktop use. Mirrors the
// API's optimistic-concurrency rule: a PUT must send the current revision.
class FakeCloudSaveClient : public ICloudSaveClient {
  public:
    CloudStatus getStatus(const std::string& token, std::uint64_t titleId,
                          CancelFlag cancelled = nullptr) override;
    CloudPull   pull(const std::string& token, std::uint64_t titleId,
                     CancelFlag cancelled = nullptr) override;
    CloudPush   push(const std::string& token, std::uint64_t titleId,
                     const std::vector<std::uint8_t>& blob,
                     const std::string& label, int revision,
                     CancelFlag cancelled = nullptr) override;

  private:
    struct Slot {
        int                       revision  = 0;
        std::string               label;
        std::vector<std::uint8_t> blob;
        std::int64_t              updatedAt = 0;
    };
    std::map<std::uint64_t, Slot> slots;
};

} // namespace thomaz
