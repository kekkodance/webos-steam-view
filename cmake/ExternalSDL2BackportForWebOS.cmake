# SDL2 backport for webOS (prebuilt tarball from webosbrew, like moonlight-tv).
#
# Fetched at CONFIGURE time (not build time): LVGL's SDL driver includes
# <SDL.h>, so the headers must exist before generate. An ExternalProject
# download lands too late and races LVGL's compile under -j.
if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

set(LIB_FILENAME "libSDL2-2.0.so.0")

if (DEFINED SDL2_BACKPORT_RELEASE)
    string(REGEX REPLACE "^release-([0-9]+\\.[0-9]+\\.[0-9]+)-webos\\.[0-9]+$" "SDL2-\\1-webos.tar.gz"
            _archive "${SDL2_BACKPORT_RELEASE}")
    set(_url "https://github.com/webosbrew/SDL-webOS/releases/download/${SDL2_BACKPORT_RELEASE}/${_archive}")
else ()
    message(FATAL_ERROR "SDL2_BACKPORT_RELEASE is not defined")
endif ()

set(_dl_dir "${CMAKE_BINARY_DIR}/sdl2-backport-dl")
set(_src_dir "${_dl_dir}/src")
set(_install_dir "${_dl_dir}/install")
if (NOT EXISTS "${_install_dir}/include/SDL.h")
    message(STATUS "Downloading SDL2 backport ${SDL2_BACKPORT_RELEASE}")
    file(DOWNLOAD "${_url}" "${_dl_dir}/sdl2.tar.gz" SHOW_PROGRESS
            STATUS _dl_status LOG _dl_log)
    list(GET _dl_status 0 _dl_code)
    if (NOT _dl_code EQUAL 0)
        message(FATAL_ERROR "SDL2 backport download failed: ${_dl_log}")
    endif ()
    file(ARCHIVE_EXTRACT INPUT "${_dl_dir}/sdl2.tar.gz" DESTINATION "${_src_dir}")
    file(COPY "${_src_dir}/" DESTINATION "${_install_dir}")
endif ()

add_library(ext_sdl2_backport_target SHARED IMPORTED GLOBAL)
set_target_properties(ext_sdl2_backport_target PROPERTIES IMPORTED_LOCATION ${_install_dir}/lib/${LIB_FILENAME})
target_include_directories(ext_sdl2_backport_target INTERFACE ${_install_dir}/include/SDL2 ${_install_dir}/include)
target_compile_definitions(ext_sdl2_backport_target INTERFACE __WEBOS__)

add_library(SDL2::SDL2 ALIAS ext_sdl2_backport_target)

set(SDL2_INCLUDE_DIRS ${_install_dir}/include/SDL2)
set(SDL2_LIBRARIES ext_sdl2_backport_target)
set(SDL2_FOUND TRUE)

if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR lib)
endif ()

set(SDL2_RUNTIME_LIBS "${_install_dir}/lib/${LIB_FILENAME}" CACHE STRING "" FORCE)
install(DIRECTORY ${_install_dir}/lib/ DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PATTERN "include" EXCLUDE PATTERN "bin" EXCLUDE PATTERN "share" EXCLUDE
        PATTERN "pkgconfig" EXCLUDE PATTERN "cmake" EXCLUDE PATTERN "*.a" EXCLUDE)
