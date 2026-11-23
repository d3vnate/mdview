// SPDX-License-Identifier: MIT
#pragma once

#include <windows.h>
#include <wrl.h>
#include <WebView2.h>

#include <string>

// Owns the WebView2 environment/controller and serves the embedded UI plus the
// current rendered Markdown from memory (no files on disk).
class WebViewHost {
public:
    WebViewHost();
    ~WebViewHost();

    WebViewHost(const WebViewHost&) = delete;
    WebViewHost& operator=(const WebViewHost&) = delete;

    bool Initialize(HWND hwnd);
    void Resize();
    void Focus();
    void UpdateTheme();

    void SetContent(const std::string& htmlFragment);
    void SetDocumentFolder(const std::wstring& folder);
    void SetZoom(double factor);
    void ExecuteScript(const std::wstring& js);

private:
    void OnEnvironmentCreated(HRESULT hr, ICoreWebView2Environment* env);
    void OnControllerCreated(HRESULT hr, ICoreWebView2Controller* controller);
    void OnWebMessage(ICoreWebView2WebMessageReceivedEventArgs* args);
    void OnWebResourceRequested(ICoreWebView2WebResourceRequestedEventArgs* args);
    void OnNavigationStarting(ICoreWebView2NavigationStartingEventArgs* args);
    void OnNewWindowRequested(ICoreWebView2NewWindowRequestedEventArgs* args);
    void OnAcceleratorKey(ICoreWebView2AcceleratorKeyPressedEventArgs* args);

    std::wstring DocUrlToPath(const std::wstring& url) const;
    void HandleExternalNavigation(const std::wstring& uri);

    HWND hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_;

    std::string content_;
    std::wstring docFolder_;
    bool ready_ = false;
    bool dark_ = false;
    double zoom_ = 1.0;
};
