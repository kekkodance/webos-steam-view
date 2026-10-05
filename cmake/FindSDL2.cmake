# SDL2 resolution, in order:
#   1. webOS cross builds: ExternalSDL2BackportForWebOS defines SDL2::SDL2
#      before this module runs  -  skip to the SDL2_net part below.
#   2. Linux hosts: system SDL2 via pkg-config.
#   3. Windows local build: the prebuilt SDL-webOS DLL + headers.
if (NOT TARGET SDL2::SDL2)
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(SYS_SDL2 IMPORTED_TARGET QUIET sdl2)
        if (TARGET PkgConfig::SYS_SDL2)
            add_library(SDL2::SDL2 ALIAS PkgConfig::SYS_SDL2)
            set(SDL2_FOUND TRUE)
        endif ()
    endif ()

    if (NOT TARGET SDL2::SDL2 AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/SDL-webOS/build/SDL2.dll")
        # local Windows build of SDL-webOS: link the DLL directly
        set(SDL2_FOUND TRUE)
        set(SDL2_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/third_party/SDL-webOS/include")
        set(SDL2_LIBRARY "${CMAKE_SOURCE_DIR}/third_party/SDL-webOS/build/SDL2.dll")
        add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
        if (WIN32)
            set_target_properties(SDL2::SDL2 PROPERTIES
                    IMPORTED_LOCATION "${SDL2_LIBRARY}"
                    IMPORTED_IMPLIB "${CMAKE_SOURCE_DIR}/third_party/SDL-webOS/build/libSDL2.dll.a")
        else ()
            set_target_properties(SDL2::SDL2 PROPERTIES IMPORTED_LOCATION "${SDL2_LIBRARY}")
        endif ()
        target_include_directories(SDL2::SDL2 INTERFACE "${SDL2_INCLUDE_DIRS}")
    endif ()

    if (NOT TARGET SDL2::SDL2)
        message(FATAL_ERROR "SDL2 not found: install libsdl2-dev (Linux) or build third_party/SDL-webOS")
    endif ()
endif ()

# SDL2_net for ihslib's UDP layer on non-Unix hosts (Windows host build only)
find_library(SDL2_NET_LIBRARY NAMES SDL2_net PATHS "${CMAKE_SOURCE_DIR}/third_party/SDL_net/build")
find_path(SDL2_NET_INCLUDE_DIR NAMES SDL_net.h
        PATHS "${CMAKE_SOURCE_DIR}/third_party/SDL_net/include" "${CMAKE_SOURCE_DIR}/third_party/sdl2-pc/SDL2")
if (SDL2_NET_LIBRARY AND SDL2_NET_INCLUDE_DIR AND NOT TARGET SDL2::SDL2net)
    add_library(SDL2::SDL2net SHARED IMPORTED GLOBAL)
    set_target_properties(SDL2::SDL2net PROPERTIES
            IMPORTED_LOCATION "${SDL2_NET_LIBRARY}"
            IMPORTED_IMPLIB "${SDL2_NET_LIBRARY}")
    target_include_directories(SDL2::SDL2net INTERFACE "${SDL2_NET_INCLUDE_DIR}")
endif ()
