#include "util.h"

#include <objbase.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>
#include <cstring>

namespace util {

std::wstring GetModulePath() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        DWORD n = GetModuleFileNameW(nullptr, &buf[0], static_cast<DWORD>(buf.size()));
        if (n == 0) return L"";
        if (n < buf.size()) {
            buf.resize(n);
            return buf;
        }
        buf.resize(buf.size() * 2);
    }
}

std::wstring GetExeDir() {
    std::wstring path = GetModulePath();
    return DirName(path);
}

std::wstring GetUserDataDir() {
    PWSTR raw = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw))) {
        dir = raw;
        CoTaskMemFree(raw);
    } else {
        dir = GetExeDir();
    }
    dir += L"\\mdview";
    CreateDirectoryW(dir.c_str(), nullptr);
    dir += L"\\WebView2";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring NormalizePath(const std::wstring& path) {
    if (path.empty()) return path;
    std::wstring p = path;
    if (p.size() >= 2 && p.front() == L'"' && p.back() == L'"') {
        p = p.substr(1, p.size() - 2);
    }
    DWORD needed = GetFullPathNameW(p.c_str(), 0, nullptr, nullptr);
    if (needed == 0) return p;
    std::wstring buf(needed, L'\0');
    DWORD n = GetFullPathNameW(p.c_str(), needed, &buf[0], nullptr);
    if (n == 0) return p;
    buf.resize(n);
    return buf;
}

std::wstring DirName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L".";
    // Drive root, e.g. "C:/" (keep this comment free of a trailing backslash:
    // a backslash at the end of a // comment continues the comment).
    if (pos == 2 && path.size() >= 3 && path[1] == L':') return path.substr(0, 3);
    return path.substr(0, pos);
}

std::wstring FileName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return path;
    return path.substr(pos + 1);
}

std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (b.empty()) return a;
    if (b.size() >= 2 && b[1] == L':') return b;  // absolute drive path
    if (b[0] == L'\\' || b[0] == L'/') return b;
    if (a.empty()) return b;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}

bool FileExists(const std::wstring& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                                nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                        &out[0], n, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                        &out[0], n);
    return out;
}

std::string ReadFileUtf8(const std::wstring& path, std::wstring* error) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD code = GetLastError();
        if (error) *error = L"Could not open the file (Windows error " + std::to_wstring(code) + L").";
        return {};
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart > 512LL * 1024 * 1024) {
        CloseHandle(h);
        if (error) *error = L"The file is too large to display.";
        return {};
    }

    std::string data;
    data.resize(static_cast<size_t>(size.QuadPart));
    size_t total = 0;
    while (total < data.size()) {
        DWORD chunk = static_cast<DWORD>(std::min<size_t>(data.size() - total, 1u << 20));
        DWORD read = 0;
        if (!ReadFile(h, &data[total], chunk, &read, nullptr) || read == 0) break;
        total += read;
    }
    CloseHandle(h);
    data.resize(total);

    // Strip / convert byte-order marks.
    if (data.size() >= 3 &&
        static_cast<unsigned char>(data[0]) == 0xEF &&
        static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF) {
        data.erase(0, 3);
    } else if (data.size() >= 2 &&
               static_cast<unsigned char>(data[0]) == 0xFF &&
               static_cast<unsigned char>(data[1]) == 0xFE) {
        size_t n = (data.size() - 2) / 2;
        std::wstring w(n, L'\0');
        if (n > 0) std::memcpy(&w[0], data.data() + 2, n * sizeof(wchar_t));
        data = WideToUtf8(w);
    } else if (data.size() >= 2 &&
               static_cast<unsigned char>(data[0]) == 0xFE &&
               static_cast<unsigned char>(data[1]) == 0xFF) {
        size_t n = (data.size() - 2) / 2;
        std::wstring w(n, L'\0');
        for (size_t i = 0; i < n; ++i) {
            unsigned lo = static_cast<unsigned char>(data[2 + i * 2]);
            unsigned hi = static_cast<unsigned char>(data[3 + i * 2]);
            w[i] = static_cast<wchar_t>((hi << 8) | lo);
        }
        data = WideToUtf8(w);
    }

    // If the bytes are not valid UTF-8 (and there was no UTF-16 BOM), assume
    // the system ANSI code page and convert it so md4c receives valid UTF-8.
    if (!data.empty()) {
        int probe = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                        data.data(), static_cast<int>(data.size()),
                                        nullptr, 0);
        if (probe <= 0) {
            int wideLen = MultiByteToWideChar(CP_ACP, 0, data.data(),
                                              static_cast<int>(data.size()), nullptr, 0);
            if (wideLen > 0) {
                std::wstring wide(static_cast<size_t>(wideLen), L'\0');
                MultiByteToWideChar(CP_ACP, 0, data.data(), static_cast<int>(data.size()),
                                    &wide[0], wideLen);
                data = WideToUtf8(wide);
            }
        }
    }
    return data;
}

std::string UrlDecode(const std::string& s) {
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = hex(s[i + 1]);
            int lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

std::string HtmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    static const char* hex = "0123456789abcdef";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '<': out += "\\u003c"; break;
            case '>': out += "\\u003e"; break;
            case '&': out += "\\u0026"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(hex[(c >> 4) & 0x0F]);
                    out.push_back(hex[c & 0x0F]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

std::string LoadResourceBytes(int id) {
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!res) return {};
    DWORD size = SizeofResource(nullptr, res);
    HGLOBAL handle = LoadResource(nullptr, res);
    if (!handle) return {};
    const void* data = LockResource(handle);
    if (!data) return {};
    return std::string(static_cast<const char*>(data), size);
}

bool IsDarkMode() {
    HKEY key = nullptr;
    DWORD value = 1;  // default to light
    DWORD type = 0;
    DWORD size = sizeof(value);
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type,
                         reinterpret_cast<BYTE*>(&value), &size);
        RegCloseKey(key);
    }
    return value == 0;
}

void OpenUrl(const std::wstring& url) {
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenWithShell(const std::wstring& target) {
    ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace util
