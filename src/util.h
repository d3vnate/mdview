// SPDX-License-Identifier: MIT
#pragma once

#include <windows.h>
#include <string>

namespace util {

// Paths -----------------------------------------------------------------
std::wstring GetModulePath();
std::wstring GetExeDir();
std::wstring GetUserDataDir();          // %LOCALAPPDATA%\mdview\WebView2
std::wstring NormalizePath(const std::wstring& path);
std::wstring DirName(const std::wstring& path);
std::wstring FileName(const std::wstring& path);
std::wstring JoinPath(const std::wstring& a, const std::wstring& b);
bool FileExists(const std::wstring& path);

// Text ------------------------------------------------------------------
std::string ReadFileUtf8(const std::wstring& path, std::wstring* error);
std::string WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);
std::string UrlDecode(const std::string& s);
std::string HtmlEscape(const std::string& s);
std::string JsonEscape(const std::string& s);

// Resources -------------------------------------------------------------
std::string LoadResourceBytes(int id);

// Shell / theme ---------------------------------------------------------
bool IsDarkMode();
void OpenUrl(const std::wstring& url);
void OpenWithShell(const std::wstring& target);

}  // namespace util
