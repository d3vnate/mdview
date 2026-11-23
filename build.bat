@echo off
setlocal enabledelayedexpansion

rem Locate CMake: PATH, a standalone install, or the copy bundled with VS.
set "CMAKE_EXE="
for /f "delims=" %%I in ('where cmake') do if not defined CMAKE_EXE set "CMAKE_EXE=%%I"

if not defined CMAKE_EXE (
    for %%P in (
        "%ProgramFiles%\CMake\bin\cmake.exe"
        "%ProgramFiles(x86)%\CMake\bin\cmake.exe"
    ) do (
        if not defined CMAKE_EXE if exist "%%~P" set "CMAKE_EXE=%%~P"
    )
)

if not defined CMAKE_EXE (
    for /d %%D in ("%ProgramFiles%\Microsoft Visual Studio\2022\*") do (
        if not defined CMAKE_EXE (
            if exist "%%~fD\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
                set "CMAKE_EXE=%%~fD\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            )
        )
    )
)

if not defined CMAKE_EXE (
    for /d %%D in ("%ProgramFiles(x86)%\Microsoft Visual Studio\2022\*") do (
        if not defined CMAKE_EXE (
            if exist "%%~fD\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
                set "CMAKE_EXE=%%~fD\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            )
        )
    )
)

if not defined CMAKE_EXE (
    echo.
    echo CMake was not found.
    echo Install CMake, or the "C++ CMake tools for Windows" component of VS Build Tools:
    echo   winget install --id Kitware.CMake -e
    echo   winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--wait --quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    echo.
    exit /b 1
)

echo Using CMake: %CMAKE_EXE%

"%CMAKE_EXE%" -S "%~dp0." -B "%~dp0build" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo.
    echo Configure failed.
    echo Visual Studio 2022 Build Tools with the "Desktop development with C++" workload is required.
    echo.
    exit /b 1
)

"%CMAKE_EXE%" --build "%~dp0build" --config Release
if errorlevel 1 (
    echo.
    echo Build failed.
    echo.
    exit /b 1
)

echo.
echo Built: %~dp0build\Release\mdview.exe
echo.
endlocal
