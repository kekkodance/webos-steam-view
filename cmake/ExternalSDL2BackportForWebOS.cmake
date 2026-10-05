# SDL2 backport for webOS, built from source (like ihsplay).
#
# Why source, not the prebuilt tarball: the tarball was compiled against
# newer Wayland headers than webOS 2 ships (wl_display_get_registry is
# missing from the TV runtime lib). Building against the NDK sysroot keeps
# the Wayland code honest about what the compositor provides.
#
# Built at CONFIGURE... no: ExternalProject at build time, with an explicit
# dependency edge so LVGL compiles after the headers land.
if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

include(ExternalProject)

set(SDL2_BACKPORT_REVISION "webOS-2.30.x" CACHE STRING "SDL-webOS branch to build")

set(_install_dir "${CMAKE_BINARY_DIR}/sdl2-backport-install")
set(LIB_FILENAME "libSDL2-2.0.so.0")

set(EXT_SDL2_TOOLCHAIN_ARGS)
if (CMAKE_TOOLCHAIN_FILE)
    list(APPEND EXT_SDL2_TOOLCHAIN_ARGS "-DCMAKE_TOOLCHAIN_FILE:string=${CMAKE_TOOLCHAIN_FILE}")
endif ()

ExternalProject_Add(ext_sdl2_backport
        GIT_REPOSITORY "https://github.com/webosbrew/SDL-webOS.git"
        GIT_TAG "${SDL2_BACKPORT_REVISION}"
        GIT_SHALLOW TRUE
        PATCH_COMMAND git -C <SOURCE_DIR> apply "${CMAKE_SOURCE_DIR}/cmake/sdl-webos-pointer-guard.patch"
        CMAKE_ARGS ${EXT_SDL2_TOOLCHAIN_ARGS}
        -DCMAKE_BUILD_TYPE:string=${CMAKE_BUILD_TYPE}
        # The NDK sysroot static libs (libdl.a, libm.a) are broken
        # (unresolvable relocations); force shared objects throughout.
        "-DCMAKE_EXE_LINKER_FLAGS=-l:libdl.so.2 -l:libm.so.6"
        "-DCMAKE_SHARED_LINKER_FLAGS=-l:libdl.so.2 -l:libm.so.6"
        -DWEBOS=ON -DSDL_OFFSCREEN=OFF -DSDL_DISKAUDIO=OFF
        -DSDL_DUMMYAUDIO=OFF -DSDL_DUMMYVIDEO=OFF -DSDL_KMSDRM=OFF
        -DSDL_VENDOR_INFO=webOS
        BUILD_BYPRODUCTS <INSTALL_DIR>/lib/${LIB_FILENAME}
        INSTALL_DIR "${_install_dir}"
        )
ExternalProject_Get_Property(ext_sdl2_backport INSTALL_DIR)

# Headers land at build time; pre-create the dirs so configure-time IMPORTED
# checks pass, and order LVGL after the build (add_dependencies in parent).
file(MAKE_DIRECTORY ${INSTALL_DIR}/include/SDL2 ${INSTALL_DIR}/include)

add_library(ext_sdl2_backport_target SHARED IMPORTED GLOBAL)
set_target_properties(ext_sdl2_backport_target PROPERTIES IMPORTED_LOCATION ${INSTALL_DIR}/lib/${LIB_FILENAME})
target_include_directories(ext_sdl2_backport_target INTERFACE ${INSTALL_DIR}/include/SDL2 ${INSTALL_DIR}/include)
target_compile_definitions(ext_sdl2_backport_target INTERFACE __WEBOS__)
add_dependencies(ext_sdl2_backport_target ext_sdl2_backport)

add_library(SDL2::SDL2 ALIAS ext_sdl2_backport_target)

set(SDL2_INCLUDE_DIRS ${INSTALL_DIR}/include/SDL2)
set(SDL2_LIBRARIES ext_sdl2_backport_target)
set(SDL2_FOUND TRUE)

if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR lib)
endif ()

set(SDL2_RUNTIME_LIBS "${INSTALL_DIR}/lib/${LIB_FILENAME}" CACHE STRING "" FORCE)
install(DIRECTORY ${INSTALL_DIR}/lib/ DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PATTERN "include" EXCLUDE PATTERN "bin" EXCLUDE PATTERN "share" EXCLUDE
        PATTERN "pkgconfig" EXCLUDE PATTERN "cmake" EXCLUDE PATTERN "*.a" EXCLUDE)
