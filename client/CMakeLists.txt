cmake_minimum_required(VERSION 3.10)
project(TobinMUDClient C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

# Core protocol layer -- portable, no Windows API calls (telnet/GMCP/
# MSDP negotiation, ANSI-to-styled-runs, minimal GMCP JSON field
# extraction). See src/core/*.c's own header comments.
add_library(tobinclient_core STATIC
    src/core/telnet_client.c
    src/core/ansi_client.c
    src/core/gmcp_json.c
    src/core/map_model.c
)
target_include_directories(tobinclient_core PUBLIC include)

if(WIN32)
    # Embeds tobinmud_icon.ico as the app's real icon resource (windres,
    # via CMAKE_RC_COMPILER in the mingw toolchain file) -- fixes Windows
    # synthesizing a generic letter-avatar icon for an icon-less exe (the
    # "titlebar just shows a T" symptom, user 2026-08-05).
    enable_language(RC)
    add_executable(TobinMUDClient WIN32
        src/win32/main.c
        tobinmud.rc
    )
    target_link_libraries(TobinMUDClient PRIVATE
        tobinclient_core
        ws2_32
        winmm
        comctl32
        wininet
        comdlg32
    )
else()
    message(WARNING "TobinMUDClient's Win32 GUI only builds for Windows -- "
        "configure with -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake "
        "to cross-compile from Linux. Only the portable core library "
        "(tobinclient_core) builds natively, for testing that layer alone.")
endif()
