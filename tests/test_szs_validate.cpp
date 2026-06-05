#include "doctest.h"
#include "platform/themes/szs_validate.hpp"
#include "SarcLib/Sarc.hpp"
#include "SarcLib/Yaz0.hpp"

#include <cstdint>
#include <vector>

using namespace thomaz;

// Helper: build a known-good minimal SARC in memory using SARC::Pack.
static std::vector<std::uint8_t> make_good_sarc() {
    SARC::SarcData data;
    data.endianness = Endianness::LittleEndian;
    data.HashOnly   = false;
    data.files["test.bin"] = {0x01, 0x02, 0x03, 0x04};
    SARC::PackedSarc packed = SARC::Pack(data);
    return packed.data;
}

// Helper: build a Yaz0-compressed wrap of a good SARC.
static std::vector<std::uint8_t> make_yaz0_good_sarc() {
    auto sarc = make_good_sarc();
    return Yaz0::Compress(sarc);
}

TEST_CASE("is_structurally_valid_szs: known-good bare SARC returns true") {
    auto good = make_good_sarc();
    CHECK(is_structurally_valid_szs(good));
}

TEST_CASE("is_structurally_valid_szs: Yaz0-compressed good SARC returns true") {
    auto yaz0 = make_yaz0_good_sarc();
    CHECK(is_structurally_valid_szs(yaz0));
}

TEST_CASE("is_structurally_valid_szs: 64-byte garbage buffer returns false") {
    std::vector<std::uint8_t> garbage(64, 0xAA);
    CHECK_FALSE(is_structurally_valid_szs(garbage));
}

TEST_CASE("is_structurally_valid_szs: 3-byte buffer (< 4) returns false") {
    std::vector<std::uint8_t> tiny{0x01, 0x02, 0x03};
    CHECK_FALSE(is_structurally_valid_szs(tiny));
}

TEST_CASE("is_structurally_valid_szs: Yaz0 magic prefix with junk body returns false") {
    // 'Y','a','z','0' magic followed by junk bytes — Yaz0::Decompress should throw
    std::vector<std::uint8_t> bad_yaz0 = {0x59, 0x61, 0x7A, 0x30};
    bad_yaz0.resize(32, 0xFF); // pad with junk
    CHECK_FALSE(is_structurally_valid_szs(bad_yaz0));
}

TEST_CASE("is_structurally_valid_szs: empty buffer returns false") {
    CHECK_FALSE(is_structurally_valid_szs({}));
}
