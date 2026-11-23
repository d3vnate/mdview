// SPDX-License-Identifier: MIT
#include <windows.h>
#include <commdlg.h>
#include <objbase.h>
#include <shellapi.h>

#include <cctype>
#include <memory>
#include <string>

#include "app_messages.h"
#include "file_watch.h"
#include "markdown.h"
#include "util.h"
#include "webview_host.h"

namespace {

HWND g_hwnd = nullptr;

struct AppState {
    WebViewHost* web = nullptr;
    FileWatcher* watcher = nullptr;
    std::wstring currentPath;
};

AppState g_app;

std::string MakeErrorHtml(const std::wstring& message) {
    return "<div class=\"error\"><h2>Unable to open file</h2><p>" +
           util::HtmlEscape(util::WideToUtf8(message)) + "</p></div>";
}

std::string WithTitle(const std::string& html, const std::wstring& title) {
    return "<template id=\"md-title\">" + util::HtmlEscape(util::WideToUtf8(title)) +
           "</template>" + html;
}

bool IsBlank(const std::string& text) {
    for (unsigned char c : text) {
        if (!std::isspace(c)) return false;
    }
    return true;
}

void OpenFile(const std::wstring& inputPath, bool watch, bool initial) {
    if (!g_app.web) return;

    const std::wstring path = util::NormalizePath(inputPath);
    std::wstring error;
    std::string markdown;

    if (!util::FileExists(path)) {
        error = L"File not found: " + inputPath;
    } else {
        markdown = util::ReadFileUtf8(path, &error);
    }

    if (!error.empty()) {
        // During live reload a transient failure (e.g. an editor replacing the
        // file) should not wipe the document we are already showing.
        if (!initial) return;
        g_app.currentPath = path;
        if (g_app.watcher) g_app.watcher->Stop();
        const std::wstring name = util::FileName(path.empty() ? inputPath : path);
        g_app.web->SetDocumentFolder(util::DirName(path.empty() ? inputPath : path));
        g_app.web->SetContent(WithTitle(MakeErrorHtml(error), name));
        SetWindowTextW(g_hwnd, (name + L" - Markdown Viewer").c_str());
        return;
    }

    std::string html;
    if (IsBlank(markdown)) {
        html = "<p class=\"empty-note\">This file is empty.</p>";
    } else {
        html = RenderMarkdown(markdown);
    }

    g_app.currentPath = path;
    const std::wstring name = util::FileName(path);

    g_app.web->SetDocumentFolder(util::DirName(path));
    g_app.web->SetContent(WithTitle(html, name));
    SetWindowTextW(g_hwnd, (name + L" \u2014 Markdown Viewer").c_str());

    if (watch && g_app.watcher) {
        g_app.watcher->Start(path, g_hwnd, kMsgReload);
    }
}

void ReloadCurrent() {
    if (g_app.currentPath.empty()) return;
    OpenFile(g_app.currentPath, false, false);
}

void OpenFileDialog() {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter =
        L"Markdown files (*.md;*.markdown;*.mdown;*.mkd)\0*.md;*.markdown;*.mdown;*.mkd\0"
        L"Text files (*.txt)\0*.txt\0"
        L"All files (*.*)\0*.*\0\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrTitle = L"Open Markdown file";
    if (GetOpenFileNameW(&ofn)) {
        OpenFile(file, true, true);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            if (g_app.web) g_app.web->Resize();
            return 0;

        case WM_DROPFILES: {
            HDROP drop = reinterpret_cast<HDROP>(wParam);
            wchar_t path[MAX_PATH * 4] = {};
            if (DragQueryFileW(drop, 0, path, ARRAYSIZE(path)) > 0) {
                OpenFile(path, true, true);
            }
            DragFinish(drop);
            return 0;
        }

        case kMsgReload:
            ReloadCurrent();
            return 0;

        case kMsgOpenDialog:
            OpenFileDialog();
            return 0;

        case kMsgOpenPath: {
            std::unique_ptr<std::wstring> path(reinterpret_cast<std::wstring*>(lParam));
            if (path) OpenFile(*path, true, true);
            return 0;
        }

        case kMsgOpenLocal: {
            std::unique_ptr<std::wstring> path(reinterpret_cast<std::wstring*>(lParam));
            if (path) util::OpenWithShell(*path);
            return 0;
        }

        case kMsgOpenUrl: {
            std::unique_ptr<std::wstring> url(reinterpret_cast<std::wstring*>(lParam));
            if (url) util::OpenUrl(*url);
            return 0;
        }

        case kMsgToggleSidebar:
            if (g_app.web) {
                g_app.web->ExecuteScript(L"window.__toggleSidebar && window.__toggleSidebar();");
            }
            return 0;

        case kMsgZoom: {
            std::unique_ptr<double> factor(reinterpret_cast<double*>(lParam));
            if (factor && g_app.web) g_app.web->SetZoom(*factor);
            return 0;
        }

        case kMsgApplyTheme:
            if (g_app.web) g_app.web->UpdateTheme();
            return 0;

        case kMsgFocus:
            if (g_app.web) g_app.web->Focus();
            return 0;

        case kMsgDropContent: {
            std::unique_ptr<DropContent> drop(reinterpret_cast<DropContent*>(lParam));
            if (drop && g_app.web) {
                std::string html;
                if (IsBlank(drop->content)) {
                    html = "<p class=\"empty-note\">This file is empty.</p>";
                } else {
                    html = RenderMarkdown(drop->content);
                }
                g_app.currentPath.clear();
                if (g_app.watcher) g_app.watcher->Stop();
                g_app.web->SetContent(WithTitle(html, drop->name));
                SetWindowTextW(g_hwnd, (drop->name + L" - Markdown Viewer").c_str());
            }
            return 0;
        }

        case WM_SETTINGCHANGE:
            if (lParam && lstrcmpW(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0) {
                if (g_app.web) g_app.web->UpdateTheme();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;

        case WM_ERASEBKGND: {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            HBRUSH brush = CreateSolidBrush(util::IsDarkMode() ? RGB(13, 17, 23) : RGB(255, 255, 255));
            FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
            DeleteObject(brush);
            return 1;
        }

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 480;
            info->ptMinTrackSize.y = 360;
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = L"MdViewWindow";
    RegisterClassExW(&windowClass);

    g_hwnd = CreateWindowExW(0, L"MdViewWindow", L"Markdown Viewer",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             1100, 800, nullptr, nullptr, instance, nullptr);
    if (!g_hwnd) {
        if (SUCCEEDED(comResult)) CoUninitialize();
        return 1;
    }

    ShowWindow(g_hwnd, showCommand);
    UpdateWindow(g_hwnd);
    DragAcceptFiles(g_hwnd, TRUE);

    int exitCode = 0;
    {
        WebViewHost web;
        FileWatcher watcher;
        g_app.web = &web;
        g_app.watcher = &watcher;

        if (!web.Initialize(g_hwnd)) {
            exitCode = 1;
        } else {
            int argc = 0;
            LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
            if (argv) {
                if (argc >= 2) {
                    OpenFile(argv[1], true, true);
                }
                LocalFree(argv);
            }

            web.UpdateTheme();

            MSG message{};
            while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            exitCode = static_cast<int>(message.wParam);
        }

        g_app.web = nullptr;
        g_app.watcher = nullptr;
    }  // WebViewHost and FileWatcher are destroyed while COM is still initialised.

    if (SUCCEEDED(comResult)) CoUninitialize();
    return exitCode;
}
