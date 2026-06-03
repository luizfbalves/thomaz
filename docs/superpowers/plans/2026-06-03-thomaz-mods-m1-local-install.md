# thomaz Mods — Fase M1: Pipeline de Install Local — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Permitir importar um arquivo de mod romfs (`.zip`/`.7z`) já presente na SD, extraí-lo para um staging por-jogo, e ativar/desativar/desinstalar (um mod ativo por jogo) via LayeredFS — tudo sem rede.

**Architecture:** Segue o split do projeto: `core/mods/` (lógica pura host-testável), `platform/mods/` (IO POSIX + libarchive), `app/` (activity Borealis). Toda lógica de risco (validação do arquivo, normalização de árvore, zip-slip, cópia/remoção de árvore) fica em unidades puras testadas no host com doctest contra um tmp dir; o wrapper libarchive e a cola que toca `/atmosphere/` ficam finos e validados por smoke no desktop.

**Tech Stack:** C++17, doctest (host tests), libarchive (`switch-libarchive` no Switch / `libarchive-dev` no desktop), Borealis (UI), CMake + tests/Makefile.

**Escopo desta fase:** M1 apenas. M2 (download GameBanana) e M3 (mapeamento curado) terão planos próprios. Reaproveita `core::title_id_hex` (db_paths) e o padrão de `save_backup_io` (cópia de árvore + teste em tmp dir).

---

## File Structure

**Criar:**
- `source/core/mods/mod_types.hpp` — structs puras (`ArchiveEntry`, `InstallPlan`, `InstallError`, `StagedMod`).
- `source/core/mods/mod_paths.hpp` / `.cpp` — helpers de caminho (staging, romfs em contents, marker de ativo).
- `source/core/mods/mod_install.hpp` / `.cpp` — `is_safe_archive_path` + `plan_install` (valida romfs, computa prefixo a remover).
- `source/platform/mods/mod_store.hpp` / `.cpp` — ops genéricas de filesystem por caminho explícito (copy_tree, remove_tree, list_subdirs, markers).
- `source/platform/mods/archive_extractor.hpp` — interface `IArchiveExtractor`.
- `source/platform/mods/libarchive_extractor.cpp` — impl real (libarchive). **Fora do build de testes.**
- `source/platform/mods/mod_actions.hpp` / `.cpp` — composições finas que tocam `/atmosphere/` (enable/disable/uninstall/list). **Fora do build de testes.**
- `source/app/mod_manager_activity.hpp` / `.cpp` — tela de gerenciamento por jogo.
- `tests/test_mod_paths.cpp`, `tests/test_mod_install.cpp`, `tests/test_mod_store.cpp`.

**Modificar:**
- `tests/Makefile` — incluir `../source/core/mods/*.cpp` e `../source/platform/mods/mod_store.cpp`.
- `CMakeLists.txt` — linkar libarchive (Switch e desktop).
- `resources/i18n/pt-BR/app.json` e `resources/i18n/en-US/app.json` — strings da tela (caminhos exatos confirmados na Task 8).
- `source/app/home_activity.cpp` (ou onde fica o roteamento do hub) — entrada para a tela de mods (confirmado na Task 8).

---

### Task 1: Tipos e helpers de caminho (`core/mods/mod_paths`)

**Files:**
- Create: `source/core/mods/mod_types.hpp`
- Create: `source/core/mods/mod_paths.hpp`
- Create: `source/core/mods/mod_paths.cpp`
- Test: `tests/test_mod_paths.cpp`
- Modify: `tests/Makefile`

- [ ] **Step 1: Criar os tipos puros**

Create `source/core/mods/mod_types.hpp`:

```cpp
#pragma once
#include <string>
#include <vector>

namespace thomaz::core {

// One entry as listed inside a mod archive (relative path, '/' separators).
struct ArchiveEntry {
    std::string path;        // e.g. "romfs/Actor/foo.bin" or "ModName/romfs/foo"
    bool is_dir = false;
};

enum class InstallError {
    None,        // plan is valid
    Empty,       // archive had no entries
    UnsafePath,  // an entry was absolute or contained a ".." segment
    NotRomfs,    // no "romfs/" directory segment found
    Ambiguous,   // entries live under more than one romfs root
};

// Result of analysing an archive's entry list.
struct InstallPlan {
    InstallError error = InstallError::Empty;
    // Prefix to strip from each entry so the remainder begins with "romfs/".
    // May be empty when entries already start at "romfs/".
    std::string strip_prefix;

    bool ok() const { return error == InstallError::None; }
};

// One mod folder present in a game's staging directory.
struct StagedMod {
    std::string name;        // folder name under the title's staging dir
    bool active = false;     // matches the title's active marker
};

} // namespace thomaz::core
```

- [ ] **Step 2: Escrever o teste que falha (paths)**

Create `tests/test_mod_paths.cpp`:

```cpp
#include "doctest.h"
#include "core/mods/mod_paths.hpp"

using namespace thomaz::core;

static constexpr std::uint64_t SMO = 0x0100000000010000ULL;

TEST_CASE("staging dir is per-title, lowercase hex, under the staging root") {
    CHECK(mod_staging_dir(SMO, "Cool Skin")
          == mod_staging_root() + "/0100000000010000/Cool Skin");
}

TEST_CASE("staging title dir has no mod name") {
    CHECK(mod_staging_title_dir(SMO)
          == mod_staging_root() + "/0100000000010000");
}

TEST_CASE("contents romfs dir is the Atmosphere LayeredFS target") {
    CHECK(sd_romfs_dir(SMO) == "/atmosphere/contents/0100000000010000/romfs");
}

TEST_CASE("active marker lives in the title staging dir") {
    CHECK(active_marker_path(SMO)
          == mod_staging_root() + "/0100000000010000/.active");
}
```

- [ ] **Step 3: Adicionar as novas fontes ao build de testes**

Modify `tests/Makefile` line `SRCS := ...`: insert `$(wildcard ../source/core/mods/*.cpp)` right after `$(wildcard ../source/core/*.cpp)`, and append `../source/platform/mods/mod_store.cpp` to the end of the platform list. Resulting line:

```make
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) $(wildcard ../source/core/mods/*.cpp) $(wildcard ../source/core/feed/*.cpp) $(wildcard ../source/core/saves/*.cpp) ../source/platform/cheat_store.cpp ../source/platform/feed/http_feed_client.cpp ../source/platform/app_settings.cpp ../source/platform/saves/fake_cloud_save_client.cpp ../source/platform/saves/http_cloud_save_client.cpp ../source/platform/saves/save_backup_io.cpp ../source/platform/mods/mod_store.cpp
```

- [ ] **Step 4: Rodar o teste e confirmar que falha**

Run: `make -C tests test`
Expected: FAIL na compilação — `core/mods/mod_paths.hpp: No such file or directory`.

- [ ] **Step 5: Implementar mod_paths**

Create `source/core/mods/mod_paths.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <string>

namespace thomaz::core {

// Root of the per-title mod staging area. SD on Switch, working-dir on desktop.
std::string mod_staging_root();

// <staging root>/<lower title id>
std::string mod_staging_title_dir(std::uint64_t title_id);

// <staging root>/<lower title id>/<mod_name>
std::string mod_staging_dir(std::uint64_t title_id, const std::string& mod_name);

// Atmosphere LayeredFS romfs target for a title (no trailing slash).
std::string sd_romfs_dir(std::uint64_t title_id);

// File that records the currently active mod name for a title.
std::string active_marker_path(std::uint64_t title_id);

} // namespace thomaz::core
```

Create `source/core/mods/mod_paths.cpp`:

```cpp
#include "core/mods/mod_paths.hpp"
#include "core/db_paths.hpp" // title_id_hex

namespace thomaz::core {

std::string mod_staging_root() {
#ifdef __SWITCH__
    return "/switch/thomaz/mods";
#else
    return "thomaz-mods";
#endif
}

std::string mod_staging_title_dir(std::uint64_t title_id) {
    return mod_staging_root() + "/" + title_id_hex(title_id, false);
}

std::string mod_staging_dir(std::uint64_t title_id, const std::string& mod_name) {
    return mod_staging_title_dir(title_id) + "/" + mod_name;
}

std::string sd_romfs_dir(std::uint64_t title_id) {
    return "/atmosphere/contents/" + title_id_hex(title_id, false) + "/romfs";
}

std::string active_marker_path(std::uint64_t title_id) {
    return mod_staging_title_dir(title_id) + "/.active";
}

} // namespace thomaz::core
```

- [ ] **Step 6: Rodar o teste e confirmar que passa**

Run: `make -C tests test`
Expected: PASS (todos os casos novos verdes; suíte existente intacta).

- [ ] **Step 7: Commit**

```bash
git add source/core/mods/mod_types.hpp source/core/mods/mod_paths.hpp source/core/mods/mod_paths.cpp tests/test_mod_paths.cpp tests/Makefile
git commit -m "feat(mods): mod path helpers and pure types"
```

---

### Task 2: Validação e planejamento do arquivo (`core/mods/mod_install`)

**Files:**
- Create: `source/core/mods/mod_install.hpp`
- Create: `source/core/mods/mod_install.cpp`
- Test: `tests/test_mod_install.cpp`

- [ ] **Step 1: Escrever o teste que falha (install plan)**

Create `tests/test_mod_install.cpp`:

```cpp
#include "doctest.h"
#include "core/mods/mod_install.hpp"

using namespace thomaz::core;

static ArchiveEntry f(const std::string& p) { return ArchiveEntry{p, false}; }
static ArchiveEntry d(const std::string& p) { return ArchiveEntry{p, true}; }

TEST_CASE("is_safe_archive_path rejects absolute and traversal paths") {
    CHECK(is_safe_archive_path("romfs/a/b.bin"));
    CHECK_FALSE(is_safe_archive_path(""));
    CHECK_FALSE(is_safe_archive_path("/etc/passwd"));
    CHECK_FALSE(is_safe_archive_path(".."));
    CHECK_FALSE(is_safe_archive_path("romfs/../../x"));
    CHECK_FALSE(is_safe_archive_path("a/../b"));
}

TEST_CASE("plan_install: archive already rooted at romfs/ needs no strip") {
    InstallPlan p = plan_install({d("romfs/"), f("romfs/Actor/foo.bin")});
    REQUIRE(p.ok());
    CHECK(p.strip_prefix == "");
}

TEST_CASE("plan_install: contents/<tid>/romfs/ is stripped down to romfs/") {
    InstallPlan p = plan_install({
        f("contents/0100000000010000/romfs/Actor/foo.bin"),
        f("contents/0100000000010000/romfs/Pack/bar.bin"),
    });
    REQUIRE(p.ok());
    CHECK(p.strip_prefix == "contents/0100000000010000/");
}

TEST_CASE("plan_install: a single wrapping folder is stripped") {
    InstallPlan p = plan_install({f("Cool Skin/romfs/tex.bin")});
    REQUIRE(p.ok());
    CHECK(p.strip_prefix == "Cool Skin/");
}

TEST_CASE("plan_install: empty archive is rejected") {
    CHECK(plan_install({}).error == InstallError::Empty);
}

TEST_CASE("plan_install: no romfs segment is NotRomfs") {
    CHECK(plan_install({f("exefs/main.npdm")}).error == InstallError::NotRomfs);
}

TEST_CASE("plan_install: unsafe entry is rejected") {
    CHECK(plan_install({f("romfs/../../evil")}).error == InstallError::UnsafePath);
}

TEST_CASE("plan_install: two different romfs roots are Ambiguous") {
    InstallPlan p = plan_install({
        f("A/romfs/x.bin"),
        f("B/romfs/y.bin"),
    });
    CHECK(p.error == InstallError::Ambiguous);
}
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `make -C tests test`
Expected: FAIL na compilação — `core/mods/mod_install.hpp: No such file or directory`.

- [ ] **Step 3: Implementar mod_install**

Create `source/core/mods/mod_install.hpp`:

```cpp
#pragma once
#include "core/mods/mod_types.hpp"
#include <string>
#include <vector>

namespace thomaz::core {

// True if `path` is a safe relative archive entry: non-empty, not absolute,
// no ".." traversal segment. (Zip-slip guard, mirrors save_backup_io.)
bool is_safe_archive_path(const std::string& path);

// Analyse an archive's entry list. Finds the single "romfs/" root and the
// prefix that must be stripped so each file lands under "romfs/...".
InstallPlan plan_install(const std::vector<ArchiveEntry>& entries);

} // namespace thomaz::core
```

Create `source/core/mods/mod_install.cpp`:

```cpp
#include "core/mods/mod_install.hpp"

namespace thomaz::core {

bool is_safe_archive_path(const std::string& path) {
    if (path.empty() || path.front() == '/')
        return false;
    std::size_t start = 0;
    while (start <= path.size()) {
        std::size_t slash = path.find('/', start);
        std::string seg = (slash == std::string::npos)
                              ? path.substr(start)
                              : path.substr(start, slash - start);
        if (seg == "..")
            return false;
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return true;
}

namespace {

// Returns the prefix up to and including the "romfs/" segment's parent, i.e.
// everything before "romfs/". Returns false in `found` if no romfs segment.
std::string romfs_strip_prefix(const std::string& path, bool& found) {
    // Look for a path segment exactly equal to "romfs".
    std::size_t start = 0;
    while (start < path.size()) {
        std::size_t slash = path.find('/', start);
        if (slash == std::string::npos)
            break; // last segment is a file name, never the romfs dir
        std::string seg = path.substr(start, slash - start);
        if (seg == "romfs") {
            found = true;
            return path.substr(0, start); // prefix before "romfs/"
        }
        start = slash + 1;
    }
    found = false;
    return {};
}

} // namespace

InstallPlan plan_install(const std::vector<ArchiveEntry>& entries) {
    InstallPlan plan;
    if (entries.empty()) {
        plan.error = InstallError::Empty;
        return plan;
    }

    bool any_romfs = false;
    bool prefix_set = false;
    std::string prefix;

    for (const ArchiveEntry& e : entries) {
        if (!is_safe_archive_path(e.path)) {
            plan.error = InstallError::UnsafePath;
            return plan;
        }
        bool found = false;
        std::string p = romfs_strip_prefix(e.path, found);
        if (!found)
            continue; // entries outside any romfs/ are ignored for the prefix
        any_romfs = true;
        if (!prefix_set) {
            prefix = p;
            prefix_set = true;
        } else if (p != prefix) {
            plan.error = InstallError::Ambiguous;
            return plan;
        }
    }

    if (!any_romfs) {
        plan.error = InstallError::NotRomfs;
        return plan;
    }

    plan.error = InstallError::None;
    plan.strip_prefix = prefix;
    return plan;
}

} // namespace thomaz::core
```

- [ ] **Step 4: Rodar o teste e confirmar que passa**

Run: `make -C tests test`
Expected: PASS (todos os casos de `test_mod_install.cpp` verdes).

- [ ] **Step 5: Commit**

```bash
git add source/core/mods/mod_install.hpp source/core/mods/mod_install.cpp tests/test_mod_install.cpp
git commit -m "feat(mods): archive validation + romfs install planning"
```

---

### Task 3: Ops de filesystem genéricas (`platform/mods/mod_store`)

Funções por caminho explícito (host-testáveis contra um tmp dir, no molde de `test_save_backup_io`). Nada toca `/atmosphere/` aqui.

**Files:**
- Create: `source/platform/mods/mod_store.hpp`
- Create: `source/platform/mods/mod_store.cpp`
- Test: `tests/test_mod_store.cpp`

- [ ] **Step 1: Escrever o teste que falha (tree ops + markers)**

Create `tests/test_mod_store.cpp`:

```cpp
#include "doctest.h"
#include "platform/mods/mod_store.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>

using namespace thomaz;

static const std::string TMP = "test-mods-tmp";

static void write_file(const std::string& path, const std::string& body) {
    std::ofstream(path, std::ios::binary) << body;
}
static std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), {});
}

TEST_CASE("copy_tree replicates a directory recursively") {
    remove_tree(TMP);
    ::mkdir(TMP.c_str(), 0777);
    ::mkdir((TMP + "/src").c_str(), 0777);
    ::mkdir((TMP + "/src/romfs").c_str(), 0777);
    write_file(TMP + "/src/romfs/a.bin", "AAA");

    std::string err;
    REQUIRE(copy_tree(TMP + "/src", TMP + "/dst", &err));
    CHECK(read_file(TMP + "/dst/romfs/a.bin") == "AAA");

    remove_tree(TMP);
}

TEST_CASE("remove_tree deletes a directory and its contents") {
    remove_tree(TMP);
    ::mkdir(TMP.c_str(), 0777);
    ::mkdir((TMP + "/x").c_str(), 0777);
    write_file(TMP + "/x/f", "z");

    REQUIRE(remove_tree(TMP + "/x"));
    struct stat st;
    CHECK(::stat((TMP + "/x").c_str(), &st) != 0); // gone

    remove_tree(TMP);
}

TEST_CASE("list_subdirs returns immediate child directory names") {
    remove_tree(TMP);
    ::mkdir(TMP.c_str(), 0777);
    ::mkdir((TMP + "/Skin A").c_str(), 0777);
    ::mkdir((TMP + "/Skin B").c_str(), 0777);
    write_file(TMP + "/loose.txt", "x"); // files are ignored

    std::vector<std::string> got = list_subdirs(TMP);
    std::sort(got.begin(), got.end());
    REQUIRE(got.size() == 2);
    CHECK(got[0] == "Skin A");
    CHECK(got[1] == "Skin B");

    remove_tree(TMP);
}

TEST_CASE("markers round-trip and clear") {
    remove_tree(TMP);
    ::mkdir(TMP.c_str(), 0777);
    std::string m = TMP + "/.active";

    CHECK_FALSE(read_marker(m).has_value());
    REQUIRE(write_marker(m, "Skin A"));
    REQUIRE(read_marker(m).has_value());
    CHECK(*read_marker(m) == "Skin A");
    REQUIRE(clear_marker(m));
    CHECK_FALSE(read_marker(m).has_value());

    remove_tree(TMP);
}
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `make -C tests test`
Expected: FAIL na compilação — `platform/mods/mod_store.hpp: No such file or directory`.

- [ ] **Step 3: Implementar mod_store**

Create `source/platform/mods/mod_store.hpp`:

```cpp
#pragma once
#include <optional>
#include <string>
#include <vector>

namespace thomaz {

// Recursively copy `src_dir` into `dst_dir` (created if missing). Returns false
// and sets *err on the first failure. Existing files at the destination are
// overwritten.
bool copy_tree(const std::string& src_dir, const std::string& dst_dir, std::string* err);

// Recursively delete `dir` and everything under it. Returns true if `dir` no
// longer exists afterwards (including when it never existed).
bool remove_tree(const std::string& dir);

// Immediate child directory names of `dir` (not recursive). Empty if `dir` is
// missing. Skips "." and "..".
std::vector<std::string> list_subdirs(const std::string& dir);

// Read/write/clear a one-line marker file. read returns nullopt when missing.
std::optional<std::string> read_marker(const std::string& path);
bool write_marker(const std::string& path, const std::string& value);
bool clear_marker(const std::string& path);

} // namespace thomaz
```

Create `source/platform/mods/mod_store.cpp`:

```cpp
#include "platform/mods/mod_store.hpp"

#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>

namespace thomaz {

namespace {

bool is_dir(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool copy_file(const std::string& src, const std::string& dst) {
    std::FILE* in = std::fopen(src.c_str(), "rb");
    if (!in)
        return false;
    std::FILE* out = std::fopen(dst.c_str(), "wb");
    if (!out) {
        std::fclose(in);
        return false;
    }
    char buf[8192];
    std::size_t n;
    bool ok = true;
    while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0) {
        if (std::fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    ok = (std::fclose(out) == 0) && ok;
    std::fclose(in);
    return ok;
}

} // namespace

bool copy_tree(const std::string& src_dir, const std::string& dst_dir, std::string* err) {
    ::mkdir(dst_dir.c_str(), 0777); // ignore EEXIST

    DIR* d = ::opendir(src_dir.c_str());
    if (!d) {
        if (err)
            *err = "cannot open " + src_dir;
        return false;
    }

    bool ok = true;
    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        std::string src = src_dir + "/" + name;
        std::string dst = dst_dir + "/" + name;
        if (is_dir(src)) {
            if (!copy_tree(src, dst, err)) {
                ok = false;
                break;
            }
        } else if (!copy_file(src, dst)) {
            if (err)
                *err = "cannot copy " + src;
            ok = false;
            break;
        }
    }

    ::closedir(d);
    return ok;
}

bool remove_tree(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (!d) {
        // Not a directory (or missing): try removing as a file; success if gone.
        ::remove(dir.c_str());
        struct stat st;
        return ::stat(dir.c_str(), &st) != 0;
    }

    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        std::string child = dir + "/" + name;
        if (is_dir(child))
            remove_tree(child);
        else
            ::remove(child.c_str());
    }
    ::closedir(d);
    ::rmdir(dir.c_str());

    struct stat st;
    return ::stat(dir.c_str(), &st) != 0;
}

std::vector<std::string> list_subdirs(const std::string& dir) {
    std::vector<std::string> out;
    DIR* d = ::opendir(dir.c_str());
    if (!d)
        return out;
    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        if (is_dir(dir + "/" + name))
            out.push_back(name);
    }
    ::closedir(d);
    return out;
}

std::optional<std::string> read_marker(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return std::nullopt;
    std::string out;
    char buf[256];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        out.append(buf, n);
    std::fclose(f);
    // Trim a single trailing newline if present.
    if (!out.empty() && out.back() == '\n')
        out.pop_back();
    return out;
}

bool write_marker(const std::string& path, const std::string& value) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    std::size_t w = std::fwrite(value.data(), 1, value.size(), f);
    bool ok = (w == value.size());
    ok = (std::fclose(f) == 0) && ok;
    return ok;
}

bool clear_marker(const std::string& path) {
    ::remove(path.c_str());
    struct stat st;
    return ::stat(path.c_str(), &st) != 0;
}

} // namespace thomaz
```

- [ ] **Step 4: Rodar o teste e confirmar que passa**

Run: `make -C tests test`
Expected: PASS (tree ops e markers verdes).

- [ ] **Step 5: Garantir que o tmp de teste é ignorado**

Run: `grep -q '^test-mods-tmp' tests/.gitignore 2>/dev/null || grep -q 'test-saves-tmp' tests/.gitignore 2>/dev/null && echo found || echo check`
Se o repo ignora `test-saves-tmp` em algum `.gitignore`, adicione `test-mods-tmp/` no mesmo arquivo (mesmo diretório). Caso contrário, adicione `tests/test-mods-tmp/` ao `.gitignore` da raiz.

- [ ] **Step 6: Commit**

```bash
git add source/platform/mods/mod_store.hpp source/platform/mods/mod_store.cpp tests/test_mod_store.cpp
git add -A -- '*.gitignore'
git commit -m "feat(mods): generic tree copy/remove + active marker store"
```

---

### Task 4: Extrator de arquivos (libarchive)

Wrapper fino sobre libarchive, aplicando `strip_prefix` do `InstallPlan` e o guarda zip-slip (`is_safe_archive_path`). **Fora do build de testes** (depende de libarchive); validado por smoke na Task 7.

**Files:**
- Create: `source/platform/mods/archive_extractor.hpp`
- Create: `source/platform/mods/libarchive_extractor.cpp`

- [ ] **Step 1: Definir a interface**

Create `source/platform/mods/archive_extractor.hpp`:

```cpp
#pragma once
#include "core/mods/mod_types.hpp"
#include <functional>
#include <string>
#include <vector>

namespace thomaz {

struct ExtractResult {
    bool ok = false;
    std::string error;       // human-readable on failure
    int files_written = 0;
};

// Reads the entry list of an archive without extracting (for plan_install).
// Returns an empty vector on read failure.
std::vector<core::ArchiveEntry> list_archive_entries(const std::string& archive_path);

// Extracts `archive_path` into `dest_dir`, stripping `strip_prefix` from each
// entry path so files land directly under dest_dir (dest_dir is expected to be
// the per-mod staging dir; entries then begin at "romfs/..."). Skips entries
// that fail the zip-slip guard. `progress` is called with (done, total) entry
// counts; total may be 0 if unknown.
ExtractResult extract_archive(const std::string& archive_path,
                              const std::string& dest_dir,
                              const std::string& strip_prefix,
                              const std::function<void(int, int)>& progress);

} // namespace thomaz
```

- [ ] **Step 2: Implementar com libarchive**

Create `source/platform/mods/libarchive_extractor.cpp`:

```cpp
#include "platform/mods/archive_extractor.hpp"
#include "core/mods/mod_install.hpp" // is_safe_archive_path

#include <archive.h>
#include <archive_entry.h>

#include <sys/stat.h>

namespace thomaz {

namespace {

void ensure_parent_dirs(const std::string& path) {
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (path[i] == '/')
            ::mkdir(path.substr(0, i).c_str(), 0777); // ignore EEXIST
    }
}

struct ArchiveCloser {
    struct archive* a;
    ~ArchiveCloser() {
        if (a) {
            archive_read_close(a);
            archive_read_free(a);
        }
    }
};

struct archive* open_archive(const std::string& path) {
    struct archive* a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    if (archive_read_open_filename(a, path.c_str(), 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return nullptr;
    }
    return a;
}

} // namespace

std::vector<core::ArchiveEntry> list_archive_entries(const std::string& archive_path) {
    std::vector<core::ArchiveEntry> out;
    struct archive* a = open_archive(archive_path);
    if (!a)
        return out;
    ArchiveCloser closer{a};

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char* name = archive_entry_pathname(entry);
        if (!name)
            continue;
        bool is_dir = archive_entry_filetype(entry) == AE_IFDIR;
        out.push_back(core::ArchiveEntry{std::string(name), is_dir});
    }
    return out;
}

ExtractResult extract_archive(const std::string& archive_path,
                              const std::string& dest_dir,
                              const std::string& strip_prefix,
                              const std::function<void(int, int)>& progress) {
    ExtractResult result;

    int total = static_cast<int>(list_archive_entries(archive_path).size());

    struct archive* a = open_archive(archive_path);
    if (!a) {
        result.error = "cannot open archive";
        return result;
    }
    ArchiveCloser closer{a};

    ::mkdir(dest_dir.c_str(), 0777);

    struct archive_entry* entry;
    int seen = 0;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        ++seen;
        if (progress)
            progress(seen, total);

        const char* raw = archive_entry_pathname(entry);
        if (!raw)
            continue;
        std::string name = raw;

        // Apply the strip prefix; skip entries outside it.
        if (!strip_prefix.empty()) {
            if (name.rfind(strip_prefix, 0) != 0)
                continue;
            name = name.substr(strip_prefix.size());
        }
        if (name.empty())
            continue;
        if (!core::is_safe_archive_path(name))
            continue; // zip-slip guard

        std::string out_path = dest_dir + "/" + name;

        if (archive_entry_filetype(entry) == AE_IFDIR) {
            ensure_parent_dirs(out_path + "/");
            ::mkdir(out_path.c_str(), 0777);
            continue;
        }

        ensure_parent_dirs(out_path);
        std::FILE* out = std::fopen(out_path.c_str(), "wb");
        if (!out) {
            result.error = "cannot write " + out_path;
            return result;
        }
        const void* buff;
        std::size_t size;
        la_int64_t offset;
        int r;
        bool wrote_ok = true;
        while ((r = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK) {
            if (std::fwrite(buff, 1, size, out) != size) {
                wrote_ok = false;
                break;
            }
        }
        std::fclose(out);
        if (!wrote_ok || r != ARCHIVE_EOF) {
            result.error = "read/write error on " + name;
            return result;
        }
        ++result.files_written;
    }

    result.ok = true;
    return result;
}

} // namespace thomaz
```

- [ ] **Step 3: Commit (compila junto com o CMake da Task 5; sem teste host)**

```bash
git add source/platform/mods/archive_extractor.hpp source/platform/mods/libarchive_extractor.cpp
git commit -m "feat(mods): libarchive extractor with strip-prefix + zip-slip guard"
```

---

### Task 5: Linkar libarchive no build (CMake)

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Adicionar libarchive ao link (Switch e desktop)**

In `CMakeLists.txt`, locate the HTTP block:

```cmake
if (PLATFORM_SWITCH)
    ...
    list(APPEND APP_PLATFORM_LIB curl mbedtls mbedx509 mbedcrypto z)
elseif (PLATFORM_DESKTOP)
    find_package(CURL REQUIRED)
    list(APPEND APP_PLATFORM_LIB CURL::libcurl)
endif ()
```

Replace it with (adds libarchive + its decompression deps; link order matters on Switch — archive before its filters):

```cmake
if (PLATFORM_SWITCH)
    # switch-curl + switch-mbedtls + switch-zlib ship in the devkitpro/devkita64
    # image's switch-portlibs meta-package (no dkp-pacman step needed). Headers
    # and libs live under $DEVKITPRO/portlibs/switch. Link order matters for
    # the static GNU linker: curl -> mbedtls -> z, then archive -> its filters.
    list(APPEND APP_PLATFORM_INCLUDE $ENV{DEVKITPRO}/portlibs/switch/include)
    link_directories($ENV{DEVKITPRO}/portlibs/switch/lib)
    list(APPEND APP_PLATFORM_LIB curl mbedtls mbedx509 mbedcrypto
                                 archive bz2 lzma z)
elseif (PLATFORM_DESKTOP)
    find_package(CURL REQUIRED)
    find_package(LibArchive REQUIRED)
    list(APPEND APP_PLATFORM_LIB CURL::libcurl LibArchive::LibArchive)
endif ()
```

- [ ] **Step 2: Confirmar a dependência no desktop**

Run: `cmake --help-module FindLibArchive >/dev/null 2>&1 && echo "FindLibArchive available" || echo "missing"`
Expected: `FindLibArchive available` (módulo padrão do CMake). Se faltar `libarchive` no sistema: `sudo apt install -y libarchive-dev`.

- [ ] **Step 3: Build desktop verde**

Run: `./scripts/build-desktop.sh`
Expected: link bem-sucedido com `libarchive` (sem erros de símbolo `archive_read_*`).

> **Nota Switch/CI:** o pacote `switch-libarchive` faz parte do `switch-portlibs`. Se o build da Action falhar por `-larchive` ausente, adicionar um passo `dkp-pacman -S --noconfirm switch-libarchive switch-bzip2 switch-xz` antes do `cmake`. Validar no workflow de build do Switch após esta task.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "build(mods): link libarchive on switch and desktop"
```

---

### Task 6: Ações de mod por jogo (`platform/mods/mod_actions`)

Composições finas que ligam `mod_paths` + `mod_store` + `archive_extractor`, tocando `/atmosphere/`. **Fora do build de testes** (depende do extractor). Validadas por smoke na Task 9.

**Files:**
- Create: `source/platform/mods/mod_actions.hpp`
- Create: `source/platform/mods/mod_actions.cpp`

- [ ] **Step 1: Definir a API**

Create `source/platform/mods/mod_actions.hpp`:

```cpp
#pragma once
#include "core/mods/mod_types.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace thomaz {

struct ModActionResult {
    bool ok = false;
    std::string error;
};

// Extract an archive on the SD into this title's staging area as a new mod
// named `mod_name`. Validates it is a romfs mod (plan_install) first.
ModActionResult import_archive(std::uint64_t title_id, const std::string& mod_name,
                               const std::string& archive_path,
                               const std::function<void(int, int)>& progress);

// Staged mods for a title, with `active` reflecting the marker.
std::vector<core::StagedMod> installed_mods(std::uint64_t title_id);

// Currently active mod name for a title, or empty if none.
std::string active_mod(std::uint64_t title_id);

// Activate `mod_name`: disable the currently active mod (if any), copy this
// mod's romfs into /atmosphere/contents/<tid>/romfs, update the marker.
ModActionResult enable_mod(std::uint64_t title_id, const std::string& mod_name);

// Remove this title's romfs from /atmosphere/contents and clear the marker.
ModActionResult disable_mod(std::uint64_t title_id);

// Delete a staged mod from the staging area. If it is the active mod, disable
// it first.
ModActionResult uninstall_mod(std::uint64_t title_id, const std::string& mod_name);

} // namespace thomaz
```

- [ ] **Step 2: Implementar**

Create `source/platform/mods/mod_actions.cpp`:

```cpp
#include "platform/mods/mod_actions.hpp"

#include "core/mods/mod_install.hpp"
#include "core/mods/mod_paths.hpp"
#include "platform/mods/archive_extractor.hpp"
#include "platform/mods/mod_store.hpp"

namespace thomaz {

using core::mod_staging_dir;
using core::mod_staging_title_dir;
using core::active_marker_path;
using core::sd_romfs_dir;

ModActionResult import_archive(std::uint64_t title_id, const std::string& mod_name,
                               const std::string& archive_path,
                               const std::function<void(int, int)>& progress) {
    ModActionResult res;

    std::vector<core::ArchiveEntry> entries = list_archive_entries(archive_path);
    core::InstallPlan plan = core::plan_install(entries);
    if (!plan.ok()) {
        res.error = "not a valid romfs mod archive";
        return res;
    }

    std::string dest = mod_staging_dir(title_id, mod_name);
    ExtractResult ex = extract_archive(archive_path, dest, plan.strip_prefix, progress);
    if (!ex.ok) {
        res.error = ex.error;
        return res;
    }
    res.ok = true;
    return res;
}

std::vector<core::StagedMod> installed_mods(std::uint64_t title_id) {
    std::string active = active_mod(title_id);
    std::vector<core::StagedMod> out;
    for (const std::string& name : list_subdirs(mod_staging_title_dir(title_id)))
        out.push_back(core::StagedMod{name, name == active});
    return out;
}

std::string active_mod(std::uint64_t title_id) {
    auto m = read_marker(active_marker_path(title_id));
    return m.value_or("");
}

ModActionResult enable_mod(std::uint64_t title_id, const std::string& mod_name) {
    ModActionResult res;

    // Disable whatever is currently applied so contents/<tid>/romfs is clean.
    ModActionResult dis = disable_mod(title_id);
    if (!dis.ok) {
        res.error = dis.error;
        return res;
    }

    std::string src = mod_staging_dir(title_id, mod_name) + "/romfs";
    std::string dst = sd_romfs_dir(title_id);
    std::string err;
    if (!copy_tree(src, dst, &err)) {
        res.error = err;
        return res;
    }
    if (!write_marker(active_marker_path(title_id), mod_name)) {
        res.error = "could not write active marker";
        return res;
    }
    res.ok = true;
    return res;
}

ModActionResult disable_mod(std::uint64_t title_id) {
    ModActionResult res;
    if (!remove_tree(sd_romfs_dir(title_id))) {
        res.error = "could not remove applied romfs";
        return res;
    }
    clear_marker(active_marker_path(title_id));
    res.ok = true;
    return res;
}

ModActionResult uninstall_mod(std::uint64_t title_id, const std::string& mod_name) {
    ModActionResult res;
    if (active_mod(title_id) == mod_name) {
        ModActionResult dis = disable_mod(title_id);
        if (!dis.ok)
            return dis;
    }
    if (!remove_tree(mod_staging_dir(title_id, mod_name))) {
        res.error = "could not remove staged mod";
        return res;
    }
    res.ok = true;
    return res;
}

} // namespace thomaz
```

- [ ] **Step 3: Build desktop verde (compila a cola; sem teste host)**

Run: `./scripts/build-desktop.sh`
Expected: compila `mod_actions.cpp` + `libarchive_extractor.cpp` sem erros.

- [ ] **Step 4: Commit**

```bash
git add source/platform/mods/mod_actions.hpp source/platform/mods/mod_actions.cpp
git commit -m "feat(mods): per-game enable/disable/import/uninstall actions"
```

---

### Task 7: Smoke do pipeline de extração no desktop

Validação manual de ponta a ponta da extração + enable/disable usando um `.zip` real, antes de construir a UI.

**Files:** nenhum (script de verificação ad-hoc).

- [ ] **Step 1: Criar um zip de mod de teste**

Run:
```bash
rm -rf /tmp/modtest && mkdir -p /tmp/modtest/romfs/Pack
echo "hello" > /tmp/modtest/romfs/Pack/file.bin
(cd /tmp/modtest && zip -r /tmp/coolskin.zip romfs >/dev/null)
echo "created /tmp/coolskin.zip"
```
Expected: `created /tmp/coolskin.zip`.

- [ ] **Step 2: Escrever um harness de smoke temporário**

Create `tests/smoke_extract.cpp` (temporário, removido no Step 5):

```cpp
#include "platform/mods/mod_actions.hpp"
#include <cstdio>

using namespace thomaz;

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: smoke <archive>\n"); return 2; }
    std::uint64_t tid = 0x0100000000010000ULL;
    auto r = import_archive(tid, "Cool Skin", argv[1], nullptr);
    std::printf("import ok=%d err=%s\n", r.ok, r.error.c_str());
    for (auto& m : installed_mods(tid))
        std::printf("  staged: %s active=%d\n", m.name.c_str(), m.active);
    auto e = enable_mod(tid, "Cool Skin");
    std::printf("enable ok=%d err=%s active=%s\n", e.ok, e.error.c_str(),
                active_mod(tid).c_str());
    return r.ok && e.ok ? 0 : 1;
}
```

- [ ] **Step 3: Compilar e rodar o smoke (desktop, com libarchive do host)**

Run:
```bash
g++ -std=c++17 -I source -I lib/json \
  tests/smoke_extract.cpp \
  source/core/db_paths.cpp source/core/mods/mod_paths.cpp \
  source/core/mods/mod_install.cpp \
  source/platform/mods/mod_store.cpp \
  source/platform/mods/libarchive_extractor.cpp \
  source/platform/mods/mod_actions.cpp \
  -larchive -o /tmp/smoke_extract && /tmp/smoke_extract /tmp/coolskin.zip
```
Expected:
```
import ok=1 err=
  staged: Cool Skin active=0
enable ok=1 err= active=Cool Skin
```

- [ ] **Step 4: Conferir a árvore aplicada e o staging**

Run:
```bash
cat thomaz-mods/0100000000010000/Cool\ Skin/romfs/Pack/file.bin
cat /atmosphere/contents/0100000000010000/romfs/Pack/file.bin 2>/dev/null \
  || echo "(no /atmosphere on desktop — expected; check the relative copy target)"
```
Expected: o staging contém `hello`. No desktop, `sd_romfs_dir` retorna um caminho absoluto `/atmosphere/...` que não existe — registrar isso como limitação conhecida do smoke desktop; o caminho de cópia real é validado em hardware na Task 9. (O importante aqui é provar extração + staging + plano romfs.)

- [ ] **Step 5: Limpar o harness e artefatos**

Run: `rm -f tests/smoke_extract.cpp /tmp/smoke_extract /tmp/coolskin.zip && rm -rf thomaz-mods /tmp/modtest`
Não comitar nada deste task (apenas validação). Se algo falhou, voltar à Task 4/6 antes de prosseguir.

---

### Task 8: Tela de gerenciamento de mods (Borealis)

UI por jogo: entra pela lista de jogos instalados, lista mods staged como toggles (um ativo por jogo), com ações importar e desinstalar. Espelha `game_list_activity` / `clear_cheats_activity`.

> **Antes de codar:** ler `source/app/clear_cheats_activity.{hpp,cpp}` e `source/app/game_list_activity.{hpp,cpp}` para copiar exatamente o padrão (construção via XML em `resources/`, injeção de serviços, i18n keys, navegação/push de activity, listagem de títulos via `ITitleService`). Confirmar o nome real do arquivo i18n (provável `resources/i18n/<lang>/app.json` ou `main.json`) e o ponto de roteamento do hub (provável `home_activity.cpp`).

**Files:**
- Create: `source/app/mod_manager_activity.hpp`
- Create: `source/app/mod_manager_activity.cpp`
- Create: `resources/xml/activity/mod_manager_activity.xml` (seguir o padrão XML das activities existentes; confirmar o diretório real ao ler as outras telas)
- Modify: `resources/i18n/pt-BR/<arquivo>.json`, `resources/i18n/en-US/<arquivo>.json`
- Modify: ponto de roteamento do hub (ex.: `source/app/home_activity.cpp`)

- [ ] **Step 1: Strings i18n**

Adicionar (nos dois idiomas, mesmas keys) o bloco `mods`:

pt-BR:
```json
"mods": {
  "title": "Mods",
  "pick_game": "Escolha um jogo",
  "no_mods": "Nenhum mod instalado para este jogo.",
  "import": "Importar arquivo (.zip/.7z)",
  "uninstall": "Desinstalar",
  "active": "Ativo",
  "enable_failed": "Falha ao ativar o mod",
  "import_failed": "Arquivo inválido ou não é um mod romfs"
}
```
en-US:
```json
"mods": {
  "title": "Mods",
  "pick_game": "Pick a game",
  "no_mods": "No mods installed for this game.",
  "import": "Import archive (.zip/.7z)",
  "uninstall": "Uninstall",
  "active": "Active",
  "enable_failed": "Failed to enable the mod",
  "import_failed": "Invalid archive or not a romfs mod"
}
```

- [ ] **Step 2: Activity header**

Create `source/app/mod_manager_activity.hpp`:

```cpp
#pragma once
#include <borealis.hpp>
#include <cstdint>
#include "platform/title.hpp"

namespace thomaz {

// Manage romfs mods for a single installed game: list staged mods as a
// one-active-per-game selection, with import and uninstall.
class ModManagerActivity : public brls::Activity {
  public:
    explicit ModManagerActivity(InstalledTitle title);

    brls::View* createContentView() override;

  private:
    void refreshList();

    InstalledTitle m_title;
    brls::Box* m_list = nullptr; // populated in refreshList()
};

} // namespace thomaz
```

- [ ] **Step 3: Activity implementation**

Create `source/app/mod_manager_activity.cpp`. Wire it to `mod_actions`:
- Constructor stores `m_title`.
- `createContentView()` builds from the XML (mirror how `clear_cheats_activity.cpp` loads its XML and binds `m_list`), sets the title to `i18n mods/title`, calls `refreshList()`.
- `refreshList()`:
  - clears `m_list`,
  - calls `installed_mods(m_title.title_id)`,
  - if empty, adds a label `i18n mods/no_mods`,
  - else for each `StagedMod` adds a `brls::BooleanCell` (or the toggle cell the other screens use) whose state is `mod.active`; on toggle:
    - if turning on → `enable_mod(title_id, mod.name)`; on failure show `i18n mods/enable_failed` and revert,
    - if turning off → `disable_mod(title_id)`,
    - then `refreshList()` so other rows reflect one-active-per-game,
  - adds an "Import" cell (`i18n mods/import`) that opens the importer (Step 4),
  - adds an "Uninstall" action per row (long-press or a detail button, matching how `clear_cheats` does multi-action) calling `uninstall_mod` then `refreshList()`.

```cpp
#include "app/mod_manager_activity.hpp"
#include "platform/mods/mod_actions.hpp"

// NOTE: copy the exact include set, XML binding macro, and cell types from
// clear_cheats_activity.cpp — those are the project's established patterns.

namespace thomaz {

ModManagerActivity::ModManagerActivity(InstalledTitle title)
    : m_title(std::move(title)) {}

brls::View* ModManagerActivity::createContentView() {
    // Mirror clear_cheats_activity.cpp: inflate the XML, bind m_list, set title.
    // brls::View* root = brls::View::createFromXMLResource("activity/mod_manager_activity.xml");
    // bind m_list from the XML id, then:
    refreshList();
    return /* root */ nullptr; // replace with the inflated root per the pattern
}

void ModManagerActivity::refreshList() {
    if (!m_list)
        return;
    m_list->clearViews();

    std::vector<core::StagedMod> mods = installed_mods(m_title.title_id);
    if (mods.empty()) {
        // add a brls::Label with i18n "mods/no_mods" (use the project's helper)
    }
    for (const core::StagedMod& mod : mods) {
        // add a toggle cell named mod.name, state = mod.active; wire the
        // enable/disable + refresh logic described above.
        (void)mod;
    }
    // add the Import cell here.
}

} // namespace thomaz
```

> Este arquivo é o único com formato dependente do padrão da UI existente; o executor DEVE espelhar `clear_cheats_activity.cpp` em vez de inventar a estrutura. As assinaturas de `mod_actions` (Task 6) são o contrato fixo a chamar.

- [ ] **Step 4: Importador de arquivo**

O importador lista arquivos `.zip`/`.7z` numa pasta de entrada `mod_staging_root() + "/_incoming"` e, ao escolher um, pede um nome (ou usa o nome do arquivo sem extensão) e chama `import_archive(title_id, name, path, progress)`, mostrando progresso e, ao fim, `refreshList()`. (Reusar o padrão de diálogo/seleção das telas existentes; sem file picker nativo no MVP — o usuário copia o arquivo para `sd:/switch/thomaz/mods/_incoming/`.)

Documentar esse caminho de entrada no `refreshList` vazio (texto de ajuda) e, na Task adicional de docs (fora deste plano), no README.

- [ ] **Step 5: Roteamento a partir do hub**

No ponto de roteamento do hub (espelhar como o card de "Trapaças" abre `game_list_activity`), adicionar a entrada "Mods" que abre a lista de jogos e, ao escolher um jogo, faz push de `ModManagerActivity(title)`. Confirmar o arquivo real ao ler `home_activity.cpp`.

- [ ] **Step 6: Build .nro verde**

Run: `cmake -B build_switch -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON && make -C build_switch thomaz.nro -j$(nproc)`
Expected: `.nro` gerado, link com `archive`/`curl`/`mbedtls` sem erros. (Se faltar `-larchive` no CI, aplicar a nota da Task 5.)

- [ ] **Step 7: Build desktop verde + suíte host**

Run: `./scripts/build-desktop.sh && make -C tests test`
Expected: ambos verdes.

- [ ] **Step 8: Commit**

```bash
git add source/app/mod_manager_activity.hpp source/app/mod_manager_activity.cpp resources/ source/app/home_activity.cpp
git commit -m "feat(mods): mod manager screen (import, toggle, uninstall)"
```

---

### Task 9: Verificação em hardware (checklist manual)

> Sem automação possível; registrar resultados. Pré-requisito: Switch com Atmosphère.

- [ ] **Step 1:** Copiar um `.zip` de mod romfs conhecido para `sd:/switch/thomaz/mods/_incoming/`.
- [ ] **Step 2:** Abrir thomaz → Mods → escolher o jogo → Importar → selecionar o arquivo. Confirmar que aparece staged em `sd:/switch/thomaz/mods/<tid>/<nome>/romfs/...`.
- [ ] **Step 3:** Ativar o mod. Confirmar que `sd:/atmosphere/contents/<tid>/romfs/...` foi populado e que `.active` contém o nome do mod.
- [ ] **Step 4:** Abrir o jogo e confirmar visualmente que o mod foi aplicado (LayeredFS).
- [ ] **Step 5:** Voltar ao thomaz, ativar um segundo mod do mesmo jogo. Confirmar que `contents/<tid>/romfs/` reflete só o segundo (um-ativo-por-jogo) e `.active` mudou.
- [ ] **Step 6:** Desativar. Confirmar que `contents/<tid>/romfs/` foi removido e `.active` sumiu.
- [ ] **Step 7:** Desinstalar um mod staged. Confirmar que some de `sd:/switch/thomaz/mods/<tid>/`.
- [ ] **Step 8:** Registrar o resultado no roadmap do README (marcar a validação em hardware da feature de mods M1).

---

## Self-Review

**Cobertura do spec (M1):**
- Extração libarchive → Task 4/5. Staging → Task 1/3/6. Enable/disable 1-por-jogo → Task 6 (+testes de primitivas em Task 3). UI de gerenciamento → Task 8. Testar com `.zip` à mão → Task 7 (desktop) + Task 9 (hardware). ✅ M2/M3 explicitamente fora de escopo. ✅
- Risco "memória/applet" e "TLS" pertencem a M2 (download); aqui só extração local de arquivo já na SD, então não bloqueiam M1. Registrado. ✅
- `title_id == program_id`: `sd_romfs_dir` usa `title_id_hex` (mesmo valor), consistente com `db_paths`. ✅

**Placeholders:** os únicos pontos sem código literal completo são o corpo da activity Borealis (Task 8) — intencional, pois deve espelhar exatamente `clear_cheats_activity.cpp` (padrão de UI existente que não dá para reproduzir às cegas). O contrato chamado (`mod_actions`) está 100% definido. Demais tasks têm código completo. ✅

**Consistência de tipos:** `InstallPlan`/`ArchiveEntry`/`StagedMod`/`InstallError` definidos na Task 1, usados igual em 2/4/6. `plan_install`, `is_safe_archive_path`, `list_archive_entries`, `extract_archive`, `copy_tree`/`remove_tree`/`list_subdirs`/`read_marker`/`write_marker`/`clear_marker`, `import_archive`/`installed_mods`/`active_mod`/`enable_mod`/`disable_mod`/`uninstall_mod` — assinaturas batem entre definição e uso. ✅

**Risco operacional conhecido:** disponibilidade de `switch-libarchive` no CI (nota na Task 5) e o padrão exato da UI Borealis (nota na Task 8) — ambos sinalizados para o executor confirmar contra o repo real.
