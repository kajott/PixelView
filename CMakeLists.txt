cmake_minimum_required (VERSION 3.15)

project (PixelView)

# set default build type
set (default_build_type "Debug")
if (NOT CMAKE_CONFIGURATION_TYPES)
    if (NOT CMAKE_BUILD_TYPE)
        set (CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "build type" FORCE)
        set_property (CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
    endif ()
    message (STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
endif ()
message (STATUS "Compiler Type: ${CMAKE_CXX_COMPILER_ID}")

# set C/C++ language standards for compilers that respect them
set (CMAKE_C_STANDARD 99)
set (CMAKE_CXX_STANDARD 11)

# use the static C library with MSVC builds
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

# add an option to force creating a binary that does not open a console window
if (MSVC)
    option (FORCE_NO_CONSOLE "force building a Windows-mode executable" OFF)
endif ()


###############################################################################
## THIRD-PARTY LIBRARIES                                                    ##
###############################################################################

# add the GLFW library with suitable options
foreach (disable_ GLFW_BUILD_EXAMPLES GLFW_BUILD_TESTS GLFW_BUILD_DOCS GLFW_INSTALL)
    option ("${disable_}" OFF)
endforeach ()
add_subdirectory (thirdparty/glfw)


add_library (pv_thirdparty STATIC
    thirdparty/glad/src/glad.c
    thirdparty/imgui/imgui.cpp
    thirdparty/imgui/imgui_demo.cpp
    thirdparty/imgui/imgui_widgets.cpp
    thirdparty/imgui/imgui_tables.cpp
    thirdparty/imgui/imgui_draw.cpp
    thirdparty/imgui/backends/imgui_impl_glfw.cpp
    thirdparty/imgui/backends/imgui_impl_opengl3.cpp
    src/libs_c.c
    # include libansilove piecemeal - only the really important modules
    thirdparty/libansilove/src/fonts.c
    thirdparty/libansilove/src/output.c
    thirdparty/libansilove/src/error.c
    thirdparty/libansilove/src/drawchar.c
    thirdparty/libansilove/src/loaders/ansi.c
    thirdparty/libansilove/src/loaders/artworx.c
    thirdparty/libansilove/src/loaders/binary.c
    thirdparty/libansilove/src/loaders/icedraw.c
    thirdparty/libansilove/src/loaders/pcboard.c
    thirdparty/libansilove/src/loaders/tundra.c
    thirdparty/libansilove/src/loaders/xbin.c
    thirdparty/libansilove/compat/strndup.c
    thirdparty/libansilove/compat/strtonum.c
    thirdparty/libansilove/compat/reallocarray.c
)

target_include_directories (pv_thirdparty PUBLIC
    thirdparty/glad/include
    thirdparty/imgui
    thirdparty/imgui/backends
    thirdparty/stb
    thirdparty/libansilove/include
)

target_include_directories (pv_thirdparty PRIVATE
    thirdparty/libansilove/src
    thirdparty/libansilove/compat
    src
)

target_compile_definitions (pv_thirdparty PUBLIC
    ANSILOVE_EXTERN=extern
)

target_link_libraries (pv_thirdparty glfw)
if (WIN32)
    target_link_libraries (pv_thirdparty opengl32)
else ()
    target_link_libraries (pv_thirdparty m dl GL)
endif ()


###############################################################################
## APPLICATION                                                               ##
###############################################################################

add_executable (pixelview
    src/main.cpp
    src/app.cpp
    src/app_ui.cpp
    src/app_cfgfile.cpp
    src/gl_util.cpp
    src/string_util.cpp
    src/ansi_loader.cpp
)

target_include_directories (pixelview PRIVATE pixelview)

target_link_libraries (pixelview pv_thirdparty)

# platform-dependent additional sources and options
if (WIN32)
    target_sources (pixelview PRIVATE
        src/file_util_win32.cpp
        src/utf8.manifest
    )
    # no console in Win32 Release builds (using any of three methods)
    if (FORCE_NO_CONSOLE)
        set_target_properties (pixelview PROPERTIES WIN32_EXECUTABLE ON)
    elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties (pixelview PROPERTIES WIN32_EXECUTABLE ON)
    elseif (MSVC)
        set_target_properties (pixelview PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
    endif ()
else ()
    target_sources (pixelview PRIVATE
        src/file_util_posix.cpp
    )
    # Linux builds *require* (p)threads, otherwise very weird things can happen
    set (THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package (Threads REQUIRED)
    target_link_libraries (pixelview Threads::Threads)
endif ()


###############################################################################
## COMPILER OPTIONS                                                          ##
###############################################################################

if (NOT MSVC)
    target_compile_options (pixelview PRIVATE -Wall -Wextra -pedantic -Werror -fwrapv)
else ()
    target_compile_options (pixelview PRIVATE /W4 /WX)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (NOT MSVC)
        message (STATUS "Debug build, enabling Address Sanitizer")
        target_compile_options (pixelview PUBLIC "-fsanitize=address")
        target_compile_options (pv_thirdparty PRIVATE "-fsanitize=address")
        target_link_options (pixelview PRIVATE "-fsanitize=address")
        if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            message (STATUS "Clang Debug build, enabling Undefined Behavior Sanitizer")
            target_compile_options (pixelview PRIVATE "-fsanitize=undefined")
        endif ()
    elseif (MSVC_VERSION GREATER 1627 AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message (STATUS "Debug build and MSVC 16.8 or greater detected, enabling Address Sanitizer")
        target_compile_options (pixelview PRIVATE "/fsanitize=address")
        target_compile_options (pv_thirdparty PUBLIC "/fsanitize=address")
        target_link_options (pixelview PRIVATE "/DEBUG")
        # ASAN isn't compatible with the /RTC switch and incremental linking,
        # both of which CMake enables by default
        string (REGEX REPLACE "/RTC(su|[1su])?" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        string (REGEX REPLACE "/RTC(su|[1su])?" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        string (REGEX REPLACE "/INCREMENTAL" "" CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS}")
    endif ()
endif ()
