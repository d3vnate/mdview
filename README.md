# mdview

**Markdown, minus the ceremony.**

A dead-simple Markdown viewer for Windows 11 dev machines. Drop a `.md` on it, or double-click one. It opens instantly, renders GitHub-flavored Markdown, and gets out of the way.

![mdview](docs/screenshot.png)

## Download

Grab the latest release: **https://github.com/d3vnate/mdview/releases/latest**

Unzip `mdview-v1.0.0-win-x64.zip`, then run `mdview.exe`. It is portable — no installer, no admin rights, no background services.

**Requirements:** Windows 11 x64. WebView2 Runtime is included with Windows 11. No VC++ redistributable is needed.

## Why it's small

- **One `mdview.exe`** (~450 KB).
- **No Electron. No Node. No installer. No telemetry.**
- Markdown parsed by [md4c](https://github.com/mity/md4c) (vendored, MIT).
- Rendered by **WebView2** — already part of Windows 11.
- Static CRT. No VC++ redistributable.

## No frills

No settings. No plugins. No account. No background daemons. Just a window, a file, and Markdown.

## Features

- Open, drag-drop, command-line arg, file associations
- GitHub-flavored Markdown: tables, task lists, strikethrough, autolinks
- Outline sidebar with scroll spy
- Syntax highlighting
- Live reload on file change
- Light/dark theme that follows Windows
- Relative images and links resolve against the file
- `.md` links open in-app; web links open in your browser

## Use

```bat
mdview.exe notes.md
```

Or just run it and hit **Open**. Drag a `.md` file onto the window, or double-click one after registering associations.

### File associations (optional)

From the unzipped release folder:

```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1
```

This registers `.md`, `.markdown`, `.mdown`, and `.mkd` per-user — no admin rights. Windows 11 will not let a script silently change the default app; finish in **Settings → Apps → Default apps → Markdown Viewer → Set default**.

## Keys

`Ctrl+O` open · `Ctrl+R` / `F5` reload · `Ctrl+B` outline · `Ctrl+0` reset zoom · `Ctrl+±` zoom

## Troubleshooting

- **"Windows protected your PC" (SmartScreen).** The release exe is unsigned, so Windows may warn on first launch. Click **More info → Run anyway**. Verify the download against `SHA256SUMS.txt` if you want extra assurance.
- **"The Microsoft Edge WebView2 Runtime could not be started."** Rare on Windows 11, but some stripped or enterprise images omit it. Install the Evergreen WebView2 Runtime from Microsoft, then relaunch: <https://go.microsoft.com/fwlink/p/?LinkId=2124703>
- **ARM64 PC.** The release is **x64**. Windows 11 ARM64 runs it under emulation. A native ARM64 build is not published yet.
- **A `.md` file still opens in another app.** Run `install.ps1` (above), then set mdview as the default in Windows Settings. Downloaded scripts are marked by Windows; running with `-ExecutionPolicy Bypass` avoids the policy block.
- **Antivirus flags the exe.** It is a small unsigned native binary. Check the SHA-256 against the release checksums, or build it yourself from source.
- **A missing or locked file shows an error page.** That is intentional — the app stays open instead of crashing. Use **Open** or drag-drop to pick another file.
- **Relative images or live reload stop working after a drop.** This only affects the fallback path where Windows hides the dropped file's real path. Open the file with **Open** (or via its path) to restore relative assets and live reload.

### Verify a download

```powershell
Get-FileHash .\mdview-v1.0.0-win-x64.zip -Algorithm SHA256
```

Compare the output with `SHA256SUMS.txt` from the same release.

## Build

Requires Windows 11, **VS 2022 Build Tools** (Desktop development with C++), and CMake 3.20+.

```bat
build.bat
```

Output: `build\Release\mdview.exe`. The first configure downloads the WebView2 SDK (build-time only). md4c is vendored.

### From WSL / VS Code

```bash
./build.sh
```

`build.sh` mirrors the source to a writable Windows drive, runs `build.bat`, and copies the exe back to `build/win-x64/mdview.exe`. Override the mirror with `MDVIEW_WIN_DIR`. In VS Code, `Ctrl+Shift+B` runs the same task.

Prefer native Windows? Open the project in VS Code **Windows mode** and use the `windows-msvc` CMake preset.

## License

MIT. md4c is MIT; see `third_party/md4c/LICENSE.md`.
