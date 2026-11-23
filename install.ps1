#Requires -Version 5.1
# SPDX-License-Identifier: MIT
# Registers Markdown Viewer as a handler for Markdown files under HKCU.
# No administrator rights are required.
$ErrorActionPreference = 'Stop'

$exe = Join-Path $PSScriptRoot 'mdview.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    Write-Host "mdview.exe was not found next to this script."
    Write-Host "Copy install.ps1 (and uninstall.ps1) into the folder that contains mdview.exe,"
    Write-Host "for example build\Release, and run it again."
    exit 1
}
$exe = (Resolve-Path -LiteralPath $exe).Path

$classes = 'HKCU:\Software\Classes'
$progId = 'mdview.Document'
$extensions = @('.md', '.markdown', '.mdown', '.mkd')

foreach ($ext in $extensions) {
    New-Item -Path "$classes\$ext" -Force | Out-Null
    Set-Item -Path "$classes\$ext" -Value $progId
    New-Item -Path "$classes\$ext\OpenWithProgids" -Force | Out-Null
    New-ItemProperty -Path "$classes\$ext\OpenWithProgids" -Name $progId `
        -Value ([byte[]]@()) -PropertyType Binary -Force | Out-Null
}

New-Item -Path "$classes\$progId" -Force | Out-Null
Set-Item -Path "$classes\$progId" -Value 'Markdown Document'
New-Item -Path "$classes\$progId\DefaultIcon" -Force | Out-Null
Set-Item -Path "$classes\$progId\DefaultIcon" -Value "`"$exe`",0"
New-Item -Path "$classes\$progId\shell\open\command" -Force | Out-Null
Set-Item -Path "$classes\$progId\shell\open\command" -Value "`"$exe`" `"%1`""

# Makes the app appear in the "Open with" list.
$app = "$classes\Applications\mdview.exe"
New-Item -Path "$app\shell\open\command" -Force | Out-Null
Set-Item -Path "$app\shell\open\command" -Value "`"$exe`" `"%1`""
New-Item -Path "$app\SupportedTypes" -Force | Out-Null
foreach ($ext in $extensions) {
    New-ItemProperty -Path "$app\SupportedTypes" -Name $ext -Value '' `
        -PropertyType String -Force | Out-Null
}

Write-Host ""
Write-Host "Registered Markdown Viewer for: $($extensions -join ', ')"
Write-Host ""
Write-Host "Windows 11 does not let a script silently change the default app."
Write-Host "To make it the default, open: Settings > Apps > Default apps,"
Write-Host "find 'Markdown Viewer', and choose 'Set default'."
Write-Host ""
