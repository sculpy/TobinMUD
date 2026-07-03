cmake_minimum_required(VERSION 3.10)
project(tobin_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB REQUIRED libmariadb)

file(GLOB_RECURSE TOBIN_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")

add_executable(tobin_c ${TOBIN_SOURCES})

target_include_directories(tobin_c PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${MARIADB_INCLUDE_DIRS}
)

target_link_libraries(tobin_c PRIVATE
    ${MARIADB_LIBRARIES}
    crypt
)

target_compile_options(tobin_c PRIVATE ${MARIADB_CFLAGS_OTHER})
