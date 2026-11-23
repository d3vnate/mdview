cmake_minimum_required(VERSION 3.20)

project(mdview LANGUAGES C CXX RC)

if (NOT MSVC)
    message(FATAL_ERROR "mdview targets Windows with MSVC. Build it from a Visual Studio Developer Command Prompt.")
endif ()

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "mdview must be built for x64 (the WebView2 loader path is architecture-specific).")
endif ()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statically link the C runtime so the .exe does not need the VC++ redistributable.
# If the WebView2 static loader ever reports a runtime-library mismatch, configure
# with -DMDVIEW_STATIC_CRT=OFF to link the DLL runtime instead.
option(MDVIEW_STATIC_CRT "Statically link the MSVC runtime" ON)
if (MDVIEW_STATIC_CRT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else ()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif ()

# ---------------------------------------------------------------------------
# WebView2 SDK (headers + static loader). Downloaded once from NuGet; the
# runtime itself ships with Windows 11, so nothing extra is installed for users.
# ---------------------------------------------------------------------------
set(MDVIEW_WEBVIEW2_VERSION "1.0.4370.26" CACHE STRING "Microsoft.Web.WebView2 SDK version")

set(MDVIEW_WEBVIEW2_DIR "${CMAKE_BINARY_DIR}/webview2")
set(MDVIEW_WEBVIEW2_HEADER "${MDVIEW_WEBVIEW2_DIR}/build/native/include/WebView2.h")
set(MDVIEW_WEBVIEW2_LOADER "${MDVIEW_WEBVIEW2_DIR}/build/native/x64/WebView2LoaderStatic.lib")

if (NOT EXISTS "${MDVIEW_WEBVIEW2_HEADER}" OR NOT EXISTS "${MDVIEW_WEBVIEW2_LOADER}")
    set(MDVIEW_WEBVIEW2_NUPKG "${CMAKE_BINARY_DIR}/microsoft.web.webview2.${MDVIEW_WEBVIEW2_VERSION}.nupkg")
    message(STATUS "Downloading WebView2 SDK ${MDVIEW_WEBVIEW2_VERSION}...")
    file(DOWNLOAD
        "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/${MDVIEW_WEBVIEW2_VERSION}/microsoft.web.webview2.${MDVIEW_WEBVIEW2_VERSION}.nupkg"
        "${MDVIEW_WEBVIEW2_NUPKG}"
        SHOW_PROGRESS
        TLS_VERIFY ON
        STATUS MDVIEW_DOWNLOAD_STATUS)
    list(GET MDVIEW_DOWNLOAD_STATUS 0 MDVIEW_DOWNLOAD_CODE)
    if (NOT MDVIEW_DOWNLOAD_CODE EQUAL 0)
        list(GET MDVIEW_DOWNLOAD_STATUS 1 MDVIEW_DOWNLOAD_ERROR)
        message(FATAL_ERROR "Could not download the WebView2 SDK: ${MDVIEW_DOWNLOAD_ERROR}\n"
                            "Download the NuGet package manually and extract it to ${MDVIEW_WEBVIEW2_DIR}.")
    endif ()
    file(ARCHIVE_EXTRACT INPUT "${MDVIEW_WEBVIEW2_NUPKG}" DESTINATION "${MDVIEW_WEBVIEW2_DIR}")
endif ()

if (NOT EXISTS "${MDVIEW_WEBVIEW2_HEADER}" OR NOT EXISTS "${MDVIEW_WEBVIEW2_LOADER}")
    message(FATAL_ERROR "The WebView2 SDK is incomplete under ${MDVIEW_WEBVIEW2_DIR}.")
endif ()

# ---------------------------------------------------------------------------
# Resources: embed the UI assets, icon and manifest with absolute paths so the
# RC compiler finds them regardless of the build directory.
# ---------------------------------------------------------------------------
# Use forward slashes: RC string literals treat backslashes as escapes.
set(MDVIEW_WEB_INDEX "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html")
set(MDVIEW_WEB_CSS "${CMAKE_CURRENT_SOURCE_DIR}/web/style.css")
set(MDVIEW_WEB_JS "${CMAKE_CURRENT_SOURCE_DIR}/web/app.js")
set(MDVIEW_ICON "${CMAKE_CURRENT_SOURCE_DIR}/app.ico")
set(MDVIEW_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources.rc.in"
               "${CMAKE_BINARY_DIR}/resources.rc" @ONLY)

# Recompile the resources when any embedded asset changes.
set_property(SOURCE "${CMAKE_BINARY_DIR}/resources.rc" PROPERTY OBJECT_DEPENDS
    "${MDVIEW_WEB_INDEX};${MDVIEW_WEB_CSS};${MDVIEW_WEB_JS};${MDVIEW_ICON};${MDVIEW_MANIFEST}")

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(mdview WIN32
    src/main.cpp
    src/util.cpp
    src/markdown.cpp
    src/file_watch.cpp
    src/webview_host.cpp
    third_party/md4c/md4c.c
    third_party/md4c/md4c-html.c
    third_party/md4c/entity.c
    "${CMAKE_BINARY_DIR}/resources.rc"
)

target_include_directories(mdview PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/md4c"
    "${MDVIEW_WEBVIEW2_DIR}/build/native/include"
)

target_link_libraries(mdview PRIVATE
    "${MDVIEW_WEBVIEW2_LOADER}"
    user32
    gdi32
    shell32
    ole32
    oleaut32
    uuid
    shlwapi
    dwmapi
    comdlg32
    advapi32
)

target_compile_definitions(mdview PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_compile_options(mdview PRIVATE
    $<$<COMPILE_LANGUAGE:C>:/utf-8 /W4>
    $<$<COMPILE_LANGUAGE:CXX>:/utf-8 /W4 /permissive->
)

# We embed our own manifest through resources.rc.
target_link_options(mdview PRIVATE /MANIFEST:NO)

set_target_properties(mdview PROPERTIES
    OUTPUT_NAME "mdview"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
