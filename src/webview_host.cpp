#include "webview_host.h"

#include "app_messages.h"
#include "util.h"

#include <dwmapi.h>
#include <objbase.h>
#include <shlwapi.h>
#include <wrl.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <utility>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {

// Posts a heap-allocated payload to the UI thread; frees it if posting fails.
template <typename T>
void PostOwned(HWND hwnd, UINT message, T* payload) {
    if (!hwnd || !PostMessageW(hwnd, message, 0, reinterpret_cast<LPARAM>(payload))) {
        delete payload;
    }
}

std::wstring StripQuery(const std::wstring& value) {
    size_t pos = value.find_first_of(L"?#");
    return pos == std::wstring::npos ? value : value.substr(0, pos);
}

std::wstring JsonGetString(const std::wstring& json, const std::wstring& key) {
    std::wstring quoted = L"\"" + key + L"\"";
    size_t pos = json.find(quoted);
    if (pos == std::wstring::npos) return L"";
    pos = json.find(L':', pos + quoted.size());
    if (pos == std::wstring::npos) return L"";
    ++pos;
    while (pos < json.size() && (json[pos] == L' ' || json[pos] == L'\t')) ++pos;
    if (pos >= json.size() || json[pos] != L'"') return L"";
    ++pos;

    std::wstring out;
    while (pos < json.size()) {
        wchar_t c = json[pos++];
        if (c == L'"') break;
        if (c != L'\\') {
            out += c;
            continue;
        }
        if (pos >= json.size()) break;
        wchar_t esc = json[pos++];
        switch (esc) {
            case L'"': out += L'"'; break;
            case L'\\': out += L'\\'; break;
            case L'/': out += L'/'; break;
            case L'b': out += L'\b'; break;
            case L'f': out += L'\f'; break;
            case L'n': out += L'\n'; break;
            case L'r': out += L'\r'; break;
            case L't': out += L'\t'; break;
            case L'u': {
                unsigned code = 0;
                for (int i = 0; i < 4 && pos < json.size(); ++i) {
                    wchar_t h = json[pos++];
                    code <<= 4;
                    if (h >= L'0' && h <= L'9') code |= static_cast<unsigned>(h - L'0');
                    else if (h >= L'a' && h <= L'f') code |= static_cast<unsigned>(h - L'a' + 10);
                    else if (h >= L'A' && h <= L'F') code |= static_cast<unsigned>(h - L'A' + 10);
                }
                out += static_cast<wchar_t>(code);
                break;
            }
            default: out += esc; break;
        }
    }
    return out;
}

bool HasMarkdownExtension(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return false;
    std::wstring ext = path.substr(dot + 1);
    for (wchar_t& c : ext) c = static_cast<wchar_t>(std::towlower(c));
    return ext == L"md" || ext == L"markdown" || ext == L"mdown" ||
           ext == L"mkd" || ext == L"mdx" || ext == L"txt";
}

}  // namespace

WebViewHost::WebViewHost() = default;

WebViewHost::~WebViewHost() {
    if (controller_) controller_->Close();
}

bool WebViewHost::Initialize(HWND hwnd) {
    hwnd_ = hwnd;
    std::wstring userData = util::GetUserDataDir();

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                OnEnvironmentCreated(result, env);
                return S_OK;
            })
            .Get());

    if (FAILED(hr)) {
        MessageBoxW(hwnd_,
                    L"The Microsoft Edge WebView2 Runtime could not be started.\n\n"
                    L"It is included with Windows 11, but if it is missing you can install "
                    L"the Evergreen WebView2 Runtime from Microsoft.",
                    L"Markdown Viewer", MB_ICONERROR | MB_OK);
        return false;
    }
    return true;
}

void WebViewHost::OnEnvironmentCreated(HRESULT hr, ICoreWebView2Environment* env) {
    if (FAILED(hr) || !env) {
        MessageBoxW(hwnd_, L"Failed to create the WebView2 environment.",
                    L"Markdown Viewer", MB_ICONERROR | MB_OK);
        return;
    }
    environment_ = env;

    HRESULT createHr = environment_->CreateCoreWebView2Controller(
        hwnd_,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                OnControllerCreated(result, controller);
                return S_OK;
            })
            .Get());

    if (FAILED(createHr)) {
        MessageBoxW(hwnd_, L"Failed to create the WebView2 controller.",
                    L"Markdown Viewer", MB_ICONERROR | MB_OK);
    }
}

void WebViewHost::OnControllerCreated(HRESULT hr, ICoreWebView2Controller* controller) {
    if (FAILED(hr) || !controller) {
        MessageBoxW(hwnd_, L"Failed to create the WebView2 controller.",
                    L"Markdown Viewer", MB_ICONERROR | MB_OK);
        return;
    }

    controller_ = controller;
    controller_->get_CoreWebView2(&webview_);
    if (!webview_) return;

    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(webview_->get_Settings(&settings))) {
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_IsBuiltInErrorPageEnabled(FALSE);
        settings->put_AreDefaultContextMenusEnabled(TRUE);
    }

    // Let WebView2 deliver external file drops to the page. app.js forwards the
    // dropped file's file:/// URI back to us, which preserves the real path.
    ComPtr<ICoreWebView2Controller4> controller4;
    if (SUCCEEDED(controller_.As(&controller4))) {
        controller4->put_AllowExternalDrop(TRUE);
    }

    Resize();
    controller_->put_IsVisible(TRUE);

    webview_->AddWebResourceRequestedFilter(L"https://app.local/*",
                                            COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

    EventRegistrationToken token{};
    webview_->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                OnWebResourceRequested(args);
                return S_OK;
            })
            .Get(),
        &token);

    webview_->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                OnWebMessage(args);
                return S_OK;
            })
            .Get(),
        &token);

    webview_->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                OnNavigationStarting(args);
                return S_OK;
            })
            .Get(),
        &token);

    webview_->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                OnNewWindowRequested(args);
                return S_OK;
            })
            .Get(),
        &token);

    controller_->add_AcceleratorKeyPressed(
        Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
            [this](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* args)
                -> HRESULT {
                OnAcceleratorKey(args);
                return S_OK;
            })
            .Get(),
        &token);

    if (!docFolder_.empty()) SetDocumentFolder(docFolder_);
    ready_ = true;
    webview_->Navigate(L"https://app.local/index.html");

    // Apply WebView2 theme settings and focus from the normal message loop,
    // not from inside this WebView2 callback.
    if (hwnd_) {
        PostMessageW(hwnd_, kMsgApplyTheme, 0, 0);
        PostMessageW(hwnd_, kMsgFocus, 0, 0);
    }
}

void WebViewHost::Resize() {
    if (!controller_ || !hwnd_) return;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    controller_->put_Bounds(rc);
}

void WebViewHost::Focus() {
    if (controller_) controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

void WebViewHost::SetContent(const std::string& htmlFragment) {
    content_ = htmlFragment;
    if (ready_) ExecuteScript(L"window.__reload && window.__reload();");
}

void WebViewHost::SetDocumentFolder(const std::wstring& folder) {
    docFolder_ = folder;
    if (!webview_) return;
    ComPtr<ICoreWebView2_3> webview3;
    if (SUCCEEDED(webview_.As(&webview3))) {
        webview3->SetVirtualHostNameToFolderMapping(
            L"doc.local", folder.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
    }
}

void WebViewHost::SetZoom(double factor) {
    zoom_ = std::clamp(factor, 0.25, 5.0);
    if (controller_) controller_->put_ZoomFactor(zoom_);
}

void WebViewHost::ExecuteScript(const std::wstring& js) {
    if (webview_) webview_->ExecuteScript(js.c_str(), nullptr);
}

void WebViewHost::UpdateTheme() {
    dark_ = util::IsDarkMode();

    BOOL darkFlag = dark_ ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkFlag, sizeof(darkFlag));

    if (controller_) {
        ComPtr<ICoreWebView2Controller2> controller2;
        if (SUCCEEDED(controller_.As(&controller2))) {
            COREWEBVIEW2_COLOR background{};
            background.A = 255;
            if (dark_) {
                background.R = 13;
                background.G = 17;
                background.B = 23;
            } else {
                background.R = 255;
                background.G = 255;
                background.B = 255;
            }
            controller2->put_DefaultBackgroundColor(background);
        }
    }

    if (webview_) {
        ComPtr<ICoreWebView2_13> webview13;
        if (SUCCEEDED(webview_.As(&webview13))) {
            ComPtr<ICoreWebView2Profile> profile;
            if (SUCCEEDED(webview13->get_Profile(&profile)) && profile) {
                profile->put_PreferredColorScheme(
                    dark_ ? COREWEBVIEW2_PREFERRED_COLOR_SCHEME_DARK
                          : COREWEBVIEW2_PREFERRED_COLOR_SCHEME_LIGHT);
            }
        }
    }

    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

void WebViewHost::OnWebResourceRequested(ICoreWebView2WebResourceRequestedEventArgs* args) {
    ComPtr<ICoreWebView2WebResourceRequest> request;
    if (FAILED(args->get_Request(&request)) || !request) return;

    LPWSTR uriRaw = nullptr;
    request->get_Uri(&uriRaw);
    std::wstring uri = uriRaw ? uriRaw : L"";
    if (uriRaw) CoTaskMemFree(uriRaw);

    const std::wstring prefix = L"https://app.local";
    std::wstring path = (uri.rfind(prefix, 0) == 0) ? uri.substr(prefix.size()) : uri;
    path = StripQuery(path);

    std::string body;
    std::wstring contentType;
    std::wstring extraHeaders;
    int status = 200;
    LPCWSTR reason = L"OK";

    if (path.empty() || path == L"/" || path == L"/index.html") {
        body = util::LoadResourceBytes(101);
        contentType = L"text/html; charset=utf-8";
        extraHeaders =
            L"Cache-Control: no-store\r\n"
            L"Content-Security-Policy: default-src 'none'; script-src 'self'; "
            L"style-src 'self'; img-src 'self' https://doc.local data: blob:; "
            L"font-src 'self'; connect-src 'self'; media-src https://doc.local\r\n";
    } else if (path == L"/style.css") {
        body = util::LoadResourceBytes(102);
        contentType = L"text/css; charset=utf-8";
        extraHeaders = L"Cache-Control: no-store\r\n";
    } else if (path == L"/app.js") {
        body = util::LoadResourceBytes(103);
        contentType = L"application/javascript; charset=utf-8";
        extraHeaders = L"Cache-Control: no-store\r\n";
    } else if (path == L"/content") {
        body = content_;
        contentType = L"text/html; charset=utf-8";
        extraHeaders = L"Cache-Control: no-store\r\n";
    } else {
        status = 404;
        reason = L"Not Found";
        contentType = L"text/plain; charset=utf-8";
    }

    ComPtr<IStream> stream;
    if (status == 200) {
        stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(body.data()),
                                        static_cast<UINT>(body.size())));
        if (!stream) {
            status = 500;
            reason = L"Internal Server Error";
        }
    }

    std::wstring headers = L"Content-Type: " + contentType + L"\r\n" + extraHeaders;
    ComPtr<ICoreWebView2WebResourceResponse> response;
    if (environment_) {
        HRESULT hr = environment_->CreateWebResourceResponse(stream.Get(), status, reason,
                                                             headers.c_str(), &response);
        if (SUCCEEDED(hr) && response) {
            args->put_Response(response.Get());
        }
    }
}

void WebViewHost::OnWebMessage(ICoreWebView2WebMessageReceivedEventArgs* args) {
    LPWSTR jsonRaw = nullptr;
    args->get_WebMessageAsJson(&jsonRaw);
    std::wstring json = jsonRaw ? jsonRaw : L"";
    if (jsonRaw) CoTaskMemFree(jsonRaw);

    const std::wstring type = JsonGetString(json, L"type");
    if (type == L"openDialog") {
        if (hwnd_) PostMessageW(hwnd_, kMsgOpenDialog, 0, 0);
    } else if (type == L"reload") {
        if (hwnd_) PostMessageW(hwnd_, kMsgReload, 0, 0);
    } else if (type == L"open") {
        std::wstring path = DocUrlToPath(JsonGetString(json, L"url"));
        if (!path.empty()) PostOwned(hwnd_, kMsgOpenPath, new std::wstring(path));
    } else if (type == L"openLocal") {
        std::wstring url = JsonGetString(json, L"url");
        std::wstring path = DocUrlToPath(url);
        if (!path.empty()) {
            PostOwned(hwnd_, kMsgOpenLocal, new std::wstring(path));
        } else if (!url.empty()) {
            PostOwned(hwnd_, kMsgOpenUrl, new std::wstring(url));
        }
    } else if (type == L"external") {
        std::wstring url = JsonGetString(json, L"url");
        if (!url.empty()) PostOwned(hwnd_, kMsgOpenUrl, new std::wstring(url));
    } else if (type == L"drop") {
        std::wstring path = DocUrlToPath(JsonGetString(json, L"url"));
        if (!path.empty()) {
            if (HasMarkdownExtension(path)) {
                PostOwned(hwnd_, kMsgOpenPath, new std::wstring(path));
            } else {
                PostOwned(hwnd_, kMsgOpenLocal, new std::wstring(path));
            }
        }
    } else if (type == L"dropContent") {
        std::wstring name = JsonGetString(json, L"name");
        std::string content = util::WideToUtf8(JsonGetString(json, L"content"));
        if (!name.empty()) {
            auto* payload = new DropContent;
            payload->name = name;
            payload->content = content;
            PostOwned(hwnd_, kMsgDropContent, payload);
        }
    }
}

std::wstring WebViewHost::DocUrlToPath(const std::wstring& url) const {
    std::wstring rel;
    const std::wstring prefix = L"https://doc.local/";
    if (url.rfind(prefix, 0) == 0) {
        if (docFolder_.empty()) return L"";
        rel = url.substr(prefix.size());
    } else if (url.rfind(L"file:///", 0) == 0) {
        std::string decoded = util::UrlDecode(util::WideToUtf8(url.substr(8)));
        std::wstring path = util::Utf8ToWide(decoded);
        for (wchar_t& c : path) {
            if (c == L'/') c = L'\\';
        }
        return path;
    } else {
        return L"";
    }

    rel = StripQuery(rel);
    std::string decoded = util::UrlDecode(util::WideToUtf8(rel));
    std::wstring relative = util::Utf8ToWide(decoded);
    for (wchar_t& c : relative) {
        if (c == L'/') c = L'\\';
    }
    return util::JoinPath(docFolder_, relative);
}

void WebViewHost::HandleExternalNavigation(const std::wstring& uri) {
    if (uri.rfind(L"https://doc.local/", 0) == 0 || uri.rfind(L"file:", 0) == 0) {
        std::wstring path = DocUrlToPath(uri);
        if (path.empty()) return;
        if (HasMarkdownExtension(path)) {
            PostOwned(hwnd_, kMsgOpenPath, new std::wstring(path));
        } else {
            PostOwned(hwnd_, kMsgOpenLocal, new std::wstring(path));
        }
        return;
    }

    if (uri.rfind(L"http://", 0) == 0 || uri.rfind(L"https://", 0) == 0 ||
        uri.rfind(L"mailto:", 0) == 0) {
        PostOwned(hwnd_, kMsgOpenUrl, new std::wstring(uri));
    }
}

void WebViewHost::OnNavigationStarting(ICoreWebView2NavigationStartingEventArgs* args) {
    LPWSTR uriRaw = nullptr;
    args->get_Uri(&uriRaw);
    std::wstring uri = uriRaw ? uriRaw : L"";
    if (uriRaw) CoTaskMemFree(uriRaw);

    if (uri.rfind(L"https://app.local/", 0) != 0) {
        args->put_Cancel(TRUE);
        HandleExternalNavigation(uri);
    }
}

void WebViewHost::OnNewWindowRequested(ICoreWebView2NewWindowRequestedEventArgs* args) {
    LPWSTR uriRaw = nullptr;
    args->get_Uri(&uriRaw);
    std::wstring uri = uriRaw ? uriRaw : L"";
    if (uriRaw) CoTaskMemFree(uriRaw);

    args->put_Handled(TRUE);
    HandleExternalNavigation(uri);
}

void WebViewHost::OnAcceleratorKey(ICoreWebView2AcceleratorKeyPressedEventArgs* args) {
    COREWEBVIEW2_KEY_EVENT_KIND kind;
    if (FAILED(args->get_KeyEventKind(&kind))) return;
    if (kind != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN &&
        kind != COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) {
        return;
    }

    UINT key = 0;
    args->get_VirtualKey(&key);
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    bool handled = false;
    if (ctrl && key == 'O') {
        if (hwnd_) PostMessageW(hwnd_, kMsgOpenDialog, 0, 0);
        handled = true;
    } else if ((ctrl && key == 'R') || key == VK_F5) {
        if (hwnd_) PostMessageW(hwnd_, kMsgReload, 0, 0);
        handled = true;
    } else if (ctrl && key == 'B') {
        if (hwnd_) PostMessageW(hwnd_, kMsgToggleSidebar, 0, 0);
        handled = true;
    } else if (ctrl && key == '0') {
        PostOwned(hwnd_, kMsgZoom, new double(1.0));
        handled = true;
    } else if (ctrl && (key == VK_OEM_PLUS || key == VK_ADD || key == '=')) {
        double current = zoom_;
        if (controller_) controller_->get_ZoomFactor(&current);
        PostOwned(hwnd_, kMsgZoom, new double(current * 1.1));
        handled = true;
    } else if (ctrl && (key == VK_OEM_MINUS || key == VK_SUBTRACT || key == '-')) {
        double current = zoom_;
        if (controller_) controller_->get_ZoomFactor(&current);
        PostOwned(hwnd_, kMsgZoom, new double(current / 1.1));
        handled = true;
    }

    if (handled) args->put_Handled(TRUE);
}
