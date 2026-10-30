// Optional local HUD overlay installer. It does not ship Valve or Gearbox
// assets: only the player's installed, loose sprites are read. No existing
// target is overwritten and the generated manifest is written last.
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#endif

#if defined(_WIN32) && defined(__clang__)
// Zig's MinGW import library otherwise resolves libc++'s memchr to a private
// UCRT API-set DLL which is absent on some Windows installations. Keep the
// tiny routine local, as with the proxy's other private-CRT avoidance.
extern "C" void *__cdecl memchr(const void *data, int value, size_t count) noexcept {
    const unsigned char *p = static_cast<const unsigned char *>(data);
    for (size_t i = 0; i < count; ++i) if (p[i] == static_cast<unsigned char>(value)) return const_cast<unsigned char *>(p + i);
    return nullptr;
}
#endif

struct Entry {
    std::string name, sprite;
    int res, x, y, width, height;
    size_t line;
};
struct SpriteList {
    std::vector<std::string> lines;
    std::vector<Entry> entries;
};

static bool AtExecutableDirectory() {
#ifdef _WIN32
    wchar_t path[32768];
    DWORD n = GetModuleFileNameW(nullptr, path, 32768);
    if (!n || n >= 32768) return false;
    wchar_t *slash = nullptr;
    for (wchar_t *p = path; *p; ++p) if (*p == L'\\') slash = p;
    if (!slash) return false;
    *slash = 0;
    return SetCurrentDirectoryW(path) != 0;
#else
    char path[PATH_MAX + 1];
    ssize_t n = readlink("/proc/self/exe", path, PATH_MAX);
    if (n <= 0 || n >= PATH_MAX) return false;
    path[n] = 0;
    char *slash = strrchr(path, '/');
    if (!slash) return false;
    *slash = 0;
    return chdir(path) == 0;
#endif
}

// All child paths used below are ASCII and relative to the mod directory.
// Wide Windows APIs keep a Unicode Steam library path usable.
#ifdef _WIN32
static std::wstring Wide(const std::string &s) { return std::wstring(s.begin(), s.end()); }
static FILE *OpenRead(const std::string &p) { return _wfopen(Wide(p).c_str(), L"rb"); }
#else
static FILE *OpenRead(const std::string &p) { return fopen(p.c_str(), "rb"); }
#endif

static bool ReadFile(const std::string &path, size_t limit, std::string &out) {
    FILE *f = OpenRead(path);
    if (!f) return false;
    out.clear();
    char chunk[8192];
    bool ok = true;
    while (size_t n = fread(chunk, 1, sizeof(chunk), f)) {
        if (out.size() + n > limit) { ok = false; break; }
        out.append(chunk, n);
    }
    if (ferror(f)) ok = false;
    if (fclose(f)) ok = false;
    return ok;
}

// -1 means missing, 0 means an ordinary file, 1 directory, 2 link/other.
static int PathKind(const std::string &path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesW(Wide(path).c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return -1;
    if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) return 2;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
#else
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) return -1;
    if (S_ISREG(st.st_mode)) return 0;
    if (S_ISDIR(st.st_mode)) return 1;
    return 2;
#endif
}
static bool MakeDirectory(const std::string &path) {
#ifdef _WIN32
    return CreateDirectoryW(Wide(path).c_str(), nullptr) != 0;
#else
    return mkdir(path.c_str(), 0755) == 0;
#endif
}
static bool WriteNew(const std::string &path, const std::string &bytes) {
#ifdef _WIN32
    int fd = _wopen(Wide(path).c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
    if (fd < 0) return false;
    FILE *f = _fdopen(fd, "wb");
    if (!f) { _close(fd); return false; }
#else
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) return false;
    FILE *f = fdopen(fd, "wb");
    if (!f) { close(fd); return false; }
#endif
    bool ok = fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    if (fclose(f) != 0) ok = false;
    // On I/O failure leave the incomplete new file alone: never unlink a
    // path another process could have changed. The caller reports the error.
    return ok;
}
static bool CommitManifest(const std::string &bytes) {
#ifdef _WIN32
    unsigned long pid = (unsigned long)GetCurrentProcessId();
#else
    unsigned long pid = (unsigned long)getpid();
#endif
    char tmp[80];
    snprintf(tmp, sizeof(tmp), "sprites/.chhev_manifest_%lu.tmp", pid);
    if (!WriteNew(tmp, bytes)) return false;
#ifdef _WIN32
    // MoveFileW refuses to replace an existing destination.
    bool ok = MoveFileW(Wide(tmp).c_str(), Wide("sprites/hud.txt").c_str()) != 0;
    if (!ok) DeleteFileW(Wide(tmp).c_str());
#else
    // A same-directory hard link publishes all bytes atomically, without
    // replacing an existing hud.txt, even if another process created it.
    bool ok = link(tmp, "sprites/hud.txt") == 0;
    unlink(tmp);
#endif
    return ok;
}

static bool SameOrMissing(const std::string &path, const std::string &bytes) {
    int kind = PathKind(path);
    if (kind == -1) return true;
    if (kind != 0) return false;
    std::string existing;
    return ReadFile(path, bytes.size() + 1, existing) && existing == bytes;
}

static bool SafeSpritePath(const std::string &name) {
    if (name.empty() || name.size() > 120 || name.front() == '/' || name.back() == '/') return false;
    bool previousSlash = false;
    for (char c : name) {
        const bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '/';
        if (!valid || (c == '/' && previousSlash)) return false;
        previousSlash = c == '/';
    }
    return true;
}
static bool ParseList(const std::string &input, SpriteList &out, bool allowExcessCount = false) {
    std::istringstream in(input);
    std::string line;
    int expected = -1;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (out.lines.empty() && line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
        out.lines.push_back(line);
        std::string content = line.substr(0, line.find("//"));
        std::istringstream row(content);
        std::string first;
        if (!(row >> first)) continue;
        if (expected < 0) {
            std::istringstream number(first);
            char extra;
            if (!(number >> expected) || expected < 1 || expected > 2048 || (number >> extra) || (row >> extra)) return false;
            continue;
        }
        Entry e;
        e.name = first;
        e.line = out.lines.size() - 1;
        std::string tail;
        if (!(row >> e.res >> e.sprite >> e.x >> e.y >> e.width >> e.height) || (row >> tail)) return false;
        std::replace(e.sprite.begin(), e.sprite.end(), '\\', '/');
        if (e.res != 320 && e.res != 640 && e.res != 1280 && e.res != 2560) return false;
        if (e.x < 0 || e.y < 0 || e.width <= 0 || e.height <= 0 ||
            e.x > 8192 || e.y > 8192 || e.width > 8192 || e.height > 8192) return false;
        out.entries.push_back(e);
    }
    return expected >= 0 && (out.entries.size() == (size_t)expected ||
                             (allowExcessCount && out.entries.size() < (size_t)expected));
}
static bool Selected(const std::string &name) {
    if (name == "cross" || name == "suit_full" || name == "suit_empty") return true;
    return name.size() == 8 && name.compare(0, 7, "number_") == 0 && name[7] >= '0' && name[7] <= '9';
}
static std::string Key(const Entry &e) { return e.name + ":" + std::to_string(e.res); }
static uint32_t Little32(const std::string &s, size_t offset) {
    const unsigned char *p = reinterpret_cast<const unsigned char *>(s.data() + offset);
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static bool ValidSprite(const std::string &data, const Entry &e) {
    if (data.size() < 40 || data.compare(0, 4, "IDSP") != 0 || Little32(data, 4) != 2) return false;
    uint32_t w = Little32(data, 20), h = Little32(data, 24);
    return w > 0 && h > 0 && w <= 8192 && h <= 8192 &&
           (uint32_t)e.x + (uint32_t)e.width <= w && (uint32_t)e.y + (uint32_t)e.height <= h;
}
static std::string Alias(const std::string &name) {
    // Compact names stay below the client's 64-byte szSprite field and avoid
    // collisions with Gearbox sprite sheets. Detect hash collisions below.
    uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char c : name) { hash ^= c; hash *= UINT64_C(1099511628211); }
    char buf[40];
    snprintf(buf, sizeof(buf), "chhev_%08x%08x", (unsigned)(hash >> 32), (unsigned)hash);
    return buf;
}
static bool DigitsSequential(const SpriteList &list, int res) {
    std::vector<std::string> names;
    for (const Entry &e : list.entries) if (e.res == res) names.push_back(e.name);
    auto start = std::find(names.begin(), names.end(), "number_0");
    if (start == names.end() || names.end() - start < 10) return false;
    for (int i = 0; i < 10; ++i) if (start[i] != "number_" + std::to_string(i)) return false;
    return true;
}

static int Install() {
    if (!AtExecutableDirectory()) { fprintf(stderr, "ERROR: could not locate the mod directory.\n"); return 1; }
    std::string hlText, ofText;
    if (!ReadFile("../valve/sprites/hud.txt", 2 * 1024 * 1024, hlText) ||
        !ReadFile("../gearbox/sprites/hud.txt", 2 * 1024 * 1024, ofText)) {
        fprintf(stderr, "ERROR: loose valve/sprites/hud.txt and gearbox/sprites/hud.txt are required. No files changed.\n"); return 1;
    }
    SpriteList hl, of;
    // Some shipped/custom Valve lists declare spare slots at the end. Only
    // their named source entries are read; the Gearbox list we publish must
    // have an exact count so it remains safe to parse in the client.
    if (!ParseList(hlText, hl, true) || !ParseList(ofText, of)) {
        fprintf(stderr, "ERROR: unsupported or malformed HUD list. No files changed.\n"); return 1;
    }
    std::map<std::string, Entry> hlSelected, ofSelected;
    std::set<int> resolutions;
    for (const Entry &e : hl.entries) if (Selected(e.name)) {
        if (!hlSelected.emplace(Key(e), e).second) { fprintf(stderr, "ERROR: duplicate Half-Life HUD entry.\n"); return 1; }
    }
    for (const Entry &e : of.entries) if (Selected(e.name)) {
        if (!ofSelected.emplace(Key(e), e).second) { fprintf(stderr, "ERROR: duplicate Opposing Force HUD entry.\n"); return 1; }
        resolutions.insert(e.res);
    }
    if (!resolutions.count(320) || !resolutions.count(640)) {
        fprintf(stderr, "ERROR: expected 320 and 640 HUD entries are missing. No files changed.\n"); return 1;
    }
    for (int res : resolutions) {
        if (!DigitsSequential(of, res)) { fprintf(stderr, "ERROR: non-sequential OF number sprites. No files changed.\n"); return 1; }
        for (int i = 0; i < 13; ++i) {
            std::string role = (i < 10) ? "number_" + std::to_string(i) :
                               (i == 10 ? "cross" : (i == 11 ? "suit_full" : "suit_empty"));
            std::string key = role + ":" + std::to_string(res);
            if (!hlSelected.count(key) || !ofSelected.count(key)) {
                fprintf(stderr, "ERROR: missing %s at resolution %d in one HUD list. No files changed.\n", role.c_str(), res); return 1;
            }
        }
    }
    if (PathKind("sprites") != -1 && PathKind("sprites") != 1) {
        fprintf(stderr, "ERROR: mod sprites path is not an ordinary directory. No files changed.\n"); return 1;
    }
    std::map<std::string, std::string> files, aliasSource;
    for (const auto &it : ofSelected) {
        const Entry &source = hlSelected.at(it.first);
        if (!SafeSpritePath(source.sprite)) { fprintf(stderr, "ERROR: unsafe sprite path. No files changed.\n"); return 1; }
        const std::string alias = Alias(source.sprite);
        if (aliasSource.count(alias) && aliasSource[alias] != source.sprite) {
            fprintf(stderr, "ERROR: sprite alias collision. No files changed.\n"); return 1;
        }
        aliasSource[alias] = source.sprite;
        const std::string target = "sprites/" + alias + ".spr";
        if (!files.count(target) && !ReadFile("../valve/sprites/" + source.sprite + ".spr", 32 * 1024 * 1024, files[target])) {
            fprintf(stderr, "ERROR: missing Half-Life sprite %s. No files changed.\n", source.sprite.c_str()); return 1;
        }
        if (!ValidSprite(files[target], source)) { fprintf(stderr, "ERROR: invalid sprite or rectangle %s. No files changed.\n", source.sprite.c_str()); return 1; }
        const Entry &destination = it.second;
        std::ostringstream row;
        row << destination.name << ' ' << destination.res << ' ' << alias << ' '
            << source.x << ' ' << source.y << ' ' << source.width << ' ' << source.height;
        of.lines[destination.line] = row.str();
    }
    std::string merged;
    for (const std::string &line : of.lines) merged += line + "\n";
    files["sprites/hud.txt"] = merged;
    for (const auto &file : files) if (!SameOrMissing(file.first, file.second)) {
        fprintf(stderr, "ERROR: %s already exists and differs (or is a link). No files changed.\n", file.first.c_str()); return 1;
    }
    if (PathKind("sprites") == -1 && !MakeDirectory("sprites")) {
        fprintf(stderr, "ERROR: cannot create mod sprites directory.\n"); return 1;
    }
    if (PathKind("sprites") != 1) { fprintf(stderr, "ERROR: unsafe sprites directory.\n"); return 1; }
    // Write the HUD manifest last; until then the game cannot select partial sprites.
    for (const auto &file : files) if (file.first != "sprites/hud.txt") {
        if (PathKind(file.first) == -1 && !WriteNew(file.first, file.second)) {
            fprintf(stderr, "ERROR: cannot write %s. HUD manifest was not installed.\n", file.first.c_str()); return 1;
        }
        if (PathKind(file.first) != 0 || !SameOrMissing(file.first, file.second)) {
            fprintf(stderr, "ERROR: %s changed during installation. HUD manifest was not installed.\n", file.first.c_str()); return 1;
        }
    }
    if (PathKind("sprites/hud.txt") == -1 && !CommitManifest(merged)) {
        fprintf(stderr, "ERROR: cannot publish HUD manifest; no existing file was replaced.\n"); return 1;
    }
    if (PathKind("sprites/hud.txt") != 0 || !SameOrMissing("sprites/hud.txt", merged)) {
        fprintf(stderr, "ERROR: the existing HUD manifest differs; no file was replaced.\n"); return 1;
    }
    printf("Installed local HEV health/armor/number sprites while retaining the Gearbox HUD list and weapon icons.\n");
    printf("Restart the game. No saves or original Valve/Gearbox files were modified.\n");
    return 0;
}
int main() { return Install(); }
