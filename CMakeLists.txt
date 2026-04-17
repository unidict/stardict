cmake_minimum_required(VERSION 3.14)
project(stardict VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------- Options ----------
option(STARDICT_BUILD_TESTS "Build tests" ON)
option(BUILD_SHARED_LIBS "Build shared library" OFF)

# ---------- Dependencies ----------
find_package(ZLIB REQUIRED)

# ---------- dictzip submodule ----------
set(DICTZIP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/dictzip)

# ---------- Library ----------
file(GLOB STARDICT_SOURCES src/*.c)
list(FILTER STARDICT_SOURCES EXCLUDE REGEX ".*sd_dictzip\\.c$")

set(STARDICT_PUBLIC_HEADERS
    src/sd_stardict.h
    src/sd_dictd.h
    src/sd_types.h
    src/sd_dictfile_index.h
    src/sd_stardict_ifo.h
    src/sd_stardict_res.h
    src/sd_stardict_syn.h
    src/sd_dictfile.h
    src/sd_array.h
    src/sd_port.h
)

add_library(stardict ${STARDICT_SOURCES})
target_include_directories(stardict
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
            $<INSTALL_INTERFACE:include>
)
target_link_libraries(stardict PUBLIC dictzip ZLIB::ZLIB)

# Platform-specific link libraries
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(stardict PUBLIC m)
endif()

if(WIN32)
    target_link_libraries(stardict PUBLIC ws2_32)
endif()

# MSVC: suppress POSIX function name warnings (strdup, etc.)
if(MSVC)
    target_compile_definitions(stardict PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(stardict PRIVATE /W4)
else()
    target_compile_options(stardict PRIVATE -Wall -Wextra -pedantic)
endif()

# ---------- Tests ----------
if(STARDICT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------- Install ----------
include(GNUInstallDirs)

install(TARGETS stardict
    EXPORT stardictTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES ${STARDICT_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/stardict)

install(EXPORT stardictTargets
    FILE stardictTargets.cmake
    NAMESPACE stardict::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/stardict
)
