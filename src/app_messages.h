#pragma once

#include <windows.h>
#include <string>

struct DropContent {
    std::wstring name;
    std::string content;
};

// Actions requested by WebView2 event handlers are posted to the normal UI
// message loop. Running modal dialogs or script execution directly inside a
// WebView2 callback can re-enter WebView2 and crash.
constexpr UINT kMsgReload         = WM_APP + 1;
constexpr UINT kMsgOpenDialog     = WM_APP + 2;
constexpr UINT kMsgOpenPath       = WM_APP + 3;  // payload: std::wstring*
constexpr UINT kMsgOpenLocal      = WM_APP + 4;  // payload: std::wstring*
constexpr UINT kMsgOpenUrl        = WM_APP + 5;  // payload: std::wstring*
constexpr UINT kMsgToggleSidebar  = WM_APP + 6;
constexpr UINT kMsgZoom           = WM_APP + 7;  // payload: double*
constexpr UINT kMsgFocus          = WM_APP + 8;
constexpr UINT kMsgApplyTheme     = WM_APP + 9;
constexpr UINT kMsgDropContent    = WM_APP + 10;  // payload: DropContent*
