// SPDX-License-Identifier: MIT
#include "file_watch.h"

#include "util.h"

#include <vector>

FileWatcher::~FileWatcher() {
    Stop();
}

void FileWatcher::Start(const std::wstring& filePath, HWND notifyWnd, UINT message) {
    Stop();
    running_ = true;
    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent_) {
        running_ = false;
        return;
    }
    thread_ = std::thread(&FileWatcher::Run, this, filePath, notifyWnd, message);
}

void FileWatcher::Stop() {
    running_ = false;
    if (stopEvent_) SetEvent(stopEvent_);
    if (thread_.joinable()) thread_.join();
    if (stopEvent_) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
}

void FileWatcher::Run(std::wstring filePath, HWND notifyWnd, UINT message) {
    const std::wstring dir = util::DirName(filePath);
    const std::wstring name = util::FileName(filePath);

    HANDLE dirHandle = CreateFileW(dir.c_str(), FILE_LIST_DIRECTORY,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   nullptr, OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                   nullptr);
    if (dirHandle == INVALID_HANDLE_VALUE) return;

    std::vector<BYTE> buffer(64 * 1024);
    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!overlapped.hEvent) {
        CloseHandle(dirHandle);
        return;
    }

    while (running_) {
        ResetEvent(overlapped.hEvent);
        DWORD bytes = 0;
        BOOL ok = ReadDirectoryChangesW(
            dirHandle, buffer.data(), static_cast<DWORD>(buffer.size()), FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE |
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,
            &bytes, &overlapped, nullptr);
        if (!ok) break;

        HANDLE handles[2] = {overlapped.hEvent, stopEvent_};
        DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0 + 1) break;
        if (wait != WAIT_OBJECT_0) break;

        DWORD transferred = 0;
        if (!GetOverlappedResult(dirHandle, &overlapped, &transferred, FALSE)) {
            if (GetLastError() == ERROR_OPERATION_ABORTED) break;
            continue;
        }
        if (transferred == 0) continue;

        bool relevant = false;
        BYTE* cursor = buffer.data();
        for (;;) {
            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(cursor);
            std::wstring changed(info->FileName, info->FileNameLength / sizeof(WCHAR));
            if (lstrcmpiW(changed.c_str(), name.c_str()) == 0) {
                relevant = true;
                break;
            }
            if (info->NextEntryOffset == 0) break;
            cursor += info->NextEntryOffset;
        }

        if (relevant) {
            // Coalesce bursts of events (editors often write several times).
            if (WaitForSingleObject(stopEvent_, 200) == WAIT_OBJECT_0) break;
            if (running_ && IsWindow(notifyWnd)) {
                PostMessageW(notifyWnd, message, 0, 0);
            }
        }
    }

    if (overlapped.hEvent) CloseHandle(overlapped.hEvent);
    CancelIo(dirHandle);
    CloseHandle(dirHandle);
}
