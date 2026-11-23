#Requires -Version 5.1
# SPDX-License-Identifier: MIT
# Removes the per-user file associations created by install.ps1.
$ErrorActionPreference = 'Stop'

$classes = 'HKCU:\Software\Classes'
$progId = 'mdview.Document'
$extensions = @('.md', '.markdown', '.mdown', '.mkd')

foreach ($ext in $extensions) {
    $openWith = "$classes\$ext\OpenWithProgids"
    if (Test-Path -LiteralPath $openWith) {
        Remove-ItemProperty -Path $openWith -Name $progId -ErrorAction SilentlyContinue
    }

    $key = "$classes\$ext"
    if (Test-Path -LiteralPath $key) {
        $current = (Get-ItemProperty -Path $key -ErrorAction SilentlyContinue).'(default)'
        if ($current -eq $progId) {
            Set-Item -Path $key -Value ''
        }
    }
}

Remove-Item -Path "$classes\$progId" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "$classes\Applications\mdview.exe" -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "Removed Markdown Viewer file associations."
Write-Host "If it was the default app, Windows will ask you to choose another one next time."
