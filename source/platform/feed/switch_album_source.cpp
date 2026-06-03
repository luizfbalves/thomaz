#include "platform/feed/switch_album_source.hpp"

#ifdef __SWITCH__
#include <switch.h>
#include <cstring>
#include <vector>

namespace thomaz {

namespace {
std::string encodeId(const CapsAlbumFileId& fid) {
    const auto* p = reinterpret_cast<const unsigned char*>(&fid);
    static const char* hex = "0123456789abcdef";
    std::string s; s.reserve(sizeof(fid) * 2);
    for (size_t i = 0; i < sizeof(fid); ++i) { s += hex[p[i] >> 4]; s += hex[p[i] & 0xF]; }
    return s;
}
bool decodeId(const std::string& s, CapsAlbumFileId& out) {
    if (s.size() != sizeof(out) * 2) return false;
    auto nyb = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    auto* p = reinterpret_cast<unsigned char*>(&out);
    for (size_t i = 0; i < sizeof(out); ++i) {
        int hi = nyb(s[i*2]), lo = nyb(s[i*2+1]);
        if (hi < 0 || lo < 0) return false;
        p[i] = static_cast<unsigned char>((hi << 4) | lo);
    }
    return true;
}
} // namespace

bool SwitchAlbumSource::init()
{
    Result rc = capsaInitialize();
    return R_SUCCEEDED(rc);
}

void SwitchAlbumSource::exit()
{
    capsaExit();
}

std::vector<AlbumEntry> SwitchAlbumSource::list()
{
    std::vector<AlbumEntry> out;

    for (auto storage : { CapsAlbumStorage_Sd, CapsAlbumStorage_Nand }) {
        u64 count = 0;
        if (R_FAILED(capsaGetAlbumFileCount(storage, &count)) || count == 0)
            continue;

        std::vector<CapsAlbumEntry> entries(count);
        u64 got = 0;
        // 4º arg de capsaGetAlbumFileList é a CONTAGEM de entradas do buffer,
        // não o tamanho em bytes. (Verificar na build do Switch contra o
        // capsa.h do libnx instalado.)
        if (R_FAILED(capsaGetAlbumFileList(storage, &got, entries.data(),
                                           entries.size())))
            continue;

        for (u64 i = 0; i < got; ++i) {
            const CapsAlbumFileId& fid = entries[i].file_id;
            if (fid.content != CapsAlbumFileContents_ScreenShot)
                continue;

            AlbumEntry e;
            e.id       = encodeId(fid);
            e.titleId  = fid.application_id;
            e.captured = { fid.datetime.year, fid.datetime.month, fid.datetime.day,
                           fid.datetime.hour, fid.datetime.minute, fid.datetime.second };
            out.push_back(std::move(e));
        }
    }
    return out;
}

std::vector<std::uint8_t> SwitchAlbumSource::loadFull(const std::string& id)
{
    CapsAlbumFileId fid;
    if (!decodeId(id, fid))
        return {};

    std::vector<std::uint8_t> buf(4 * 1024 * 1024);
    u64 outSize = 0;
    Result rc = capsaLoadAlbumFile(&fid, &outSize, buf.data(), buf.size());
    if (R_FAILED(rc))
        return {};
    buf.resize(outSize);
    return buf;
}

} // namespace thomaz
#endif // __SWITCH__
