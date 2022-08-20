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
)

target_include_directories (pv_thirdparty PUBLIC
    thirdparty/glad/include
    thirdparty/imgui
    thirdparty/imgui/backends
    thirdparty/stb
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
    src/app.cpp
    src/app_ui.cpp
    src/app_cfgfile.cpp
    src/gl_util.cpp
    src/string_util.cpp
)

target_include_directories (pixelview PRIVATE pixelview)

target_link_libraries (pixelview pv_thirdparty)

# platform-dependent additional sources and options
if (WIN32)
    # no console in Win32 Release builds
    # (disabled for now -- we need a WinMain() function first!)
    #if (CMAKE_BUILD_TYPE STREQUAL "Release")
    #    set_target_properties (pixelview PROPERTIES WIN32_EXECUTABLE ON)
    #endif ()
else ()
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

if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
    #                                 ^^^^^^^^^^^^^ ASAN and nVidia's OpenGL driver don't mix well!
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
