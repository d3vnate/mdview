#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Builds mdview on Windows from a WSL terminal.
#
# The project needs MSVC + the Windows SDK, so it cannot be compiled inside
# WSL. This script either builds in place (when the project already lives on a
# Windows drive) or mirrors the source to a writable Windows drive and runs
# build.bat there.
set -euo pipefail

cd "$(dirname "$0")"
src_dir="$PWD"

if ! command -v cmd.exe >/dev/null 2>&1; then
    echo "error: cmd.exe not found. Run this from WSL with Windows interop enabled." >&2
    exit 1
fi

build_in_place() {
    local dir="$1"
    echo "Building in $(wslpath -w "$dir") ..."
    ( cd "$dir" && cmd.exe /c build.bat )
}

case "$src_dir" in
    /mnt/[a-zA-Z]/*)
        # Already on a Windows drive: build directly.
        build_in_place "$src_dir"
        ;;
    *)
        # On the WSL filesystem. MSBuild/CMake cannot reliably build from
        # \\wsl.localhost\..., so mirror the source to a Windows drive.
        mirror="${MDVIEW_WIN_DIR:-}"
        if [ -z "$mirror" ]; then
            for drive in /mnt/[a-zA-Z]; do
                if [ -d "$drive" ] && touch "$drive/.mdview_probe" 2>/dev/null; then
                    rm -f "$drive/.mdview_probe"
                    mirror="$drive/code/markdown_viewer"
                    break
                fi
            done
        fi

        if [ -z "$mirror" ]; then
            echo "error: no writable Windows drive found under /mnt." >&2
            echo "Set MDVIEW_WIN_DIR to a Windows-mounted directory and retry." >&2
            exit 1
        fi

        echo "Project is on the WSL filesystem."
        echo "Mirroring source to $(wslpath -w "$mirror") ..."
        mkdir -p "$mirror"
        rsync -a --delete \
            --exclude 'build/' \
            --exclude '.git/' \
            --exclude '__pycache__/' \
            "$src_dir/" "$mirror/"

        build_in_place "$mirror"

        exe="$mirror/build/Release/mdview.exe"
        if [ -f "$exe" ]; then
            mkdir -p "$src_dir/build/win-x64"
            cp "$exe" "$src_dir/build/win-x64/mdview.exe"
            echo
            echo "Windows build: $(wslpath -w "$exe")"
            echo "Copied to:     $src_dir/build/win-x64/mdview.exe"
        fi
        ;;
esac
