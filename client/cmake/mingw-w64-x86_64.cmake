# CMake toolchain file for cross-compiling the TobinMUD Client from the
# Linux droplet to 64-bit Windows, via Fedora's mingw64-gcc package
# (see client/README.md for the one-time `dnf install mingw64-gcc
# msitools` setup). Usage:
#   cmake -S client -B client/build-win64 -DCMAKE_TOOLCHAIN_FILE=client/cmake/mingw-w64-x86_64.cmake
#   cmake --build client/build-win64

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX}/sys-root/mingw)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
