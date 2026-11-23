// SPDX-License-Identifier: MIT
#pragma once

#include <windows.h>
#include <atomic>
#include <string>
#include <thread>

// Watches the directory containing a file and posts `message` to `notifyWnd`
// when that specific file changes (write/size/name/creation). Debounced.
class FileWatcher {
public:
    FileWatcher() = default;
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    void Start(const std::wstring& filePath, HWND notifyWnd, UINT message);
    void Stop();

private:
    void Run(std::wstring filePath, HWND notifyWnd, UINT message);

    std::thread thread_;
    std::atomic<bool> running_{false};
    HANDLE stopEvent_ = nullptr;
};
