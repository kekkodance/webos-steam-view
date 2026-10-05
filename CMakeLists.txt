cmake_minimum_required(VERSION 3.16)

project(steamview VERSION 0.1.0 LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

get_filename_component(CC_TARGET "${CMAKE_C_COMPILER}" NAME)
if (CC_TARGET STREQUAL "arm-webos-linux-gnueabi-gcc" OR CC_TARGET MATCHES "webos")
    set(TARGET_WEBOS ON)
endif ()

set(SOURCES
        src/ihs_glue.c
        src/font5x7.c
        src/main.c
        src/osd_fb.c)

if (TARGET_WEBOS)
    # TV: OSD menus encoded to H.264 for the LGNC plane. Remote keys via
    # SDL events (no video init: backport Wayland hangs old compositors).
    list(APPEND SOURCES src/webos/media.c src/webos/osd.c)
    include(ExternalOpenH264)
else ()
    # Host: OSD menus + decoded video in an SDL window. Same menus as TV.
    list(APPEND SOURCES
            src/host/host_video.c
            src/host/host_media.c)
    include(ExternalOpenH264)
endif ()

if (TARGET_WEBOS)
    # TV SDL: source-built backport, but events-subsystem only. Video init
    # is never called (its Wayland init hangs on old compositors). Remote
    # keys arrive as SDL_KEYDOWN with events-only init.
    set(SDL2_BACKPORT_REVISION "webOS-2.30.x")
    include(ExternalSDL2BackportForWebOS)
endif ()

if (NOT TARGET_WEBOS)
    find_package(SDL2 REQUIRED)
    # (FindSDL2 also creates SDL2::SDL2net from third_party/SDL_net when present)
endif ()
# ---- ihslib (Steam In-Home Streaming protocol) ----
# Resolve deps first so their variables are visible at this scope
find_package(MbedTLS)
find_package(protobuf-c)
set(IHSLIB_SAMPLES OFF CACHE BOOL "" FORCE)
set(IHSLIB_CODEGEN OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/ihslib)

find_package(PkgConfig REQUIRED)

if (TARGET_WEBOS)
    # ---- webOS native app (LGNC direct video) ----
    set(WEBOS_APPINFO_ID "com.kekko.steamview")
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

    find_package(WebOSUserland)  # provides LGNCOPENAPI (lgncopenapi stubs)
    set(OPUS_FOUND FALSE)
    include(ExternalOPUS)

    add_executable(steamview ${SOURCES})
    target_include_directories(steamview PRIVATE src src/lgnc/include)
    target_compile_definitions(steamview PRIVATE APPID="${WEBOS_APPINFO_ID}" TARGET_WEBOS)
    target_link_libraries(steamview PRIVATE SDL2::SDL2 ihslib ${LGNCOPENAPI_LIBRARIES} ${OPUS_LIBRARIES} ${OPENH264_LIBRARIES} -l:libdl.so.2 -l:libpthread.so.0 -l:librt.so.1 -static-libstdc++ -static-libgcc stdc++)
    target_include_directories(steamview SYSTEM PRIVATE ${LGNCOPENAPI_INCLUDE_DIRS} ${OPUS_INCLUDE_DIRS} ${OPENH264_INCLUDE_DIRS})
    # ext_opus installs headers at build time; the app must wait for them.
    if (TARGET ext_opus)
        add_dependencies(steamview ext_opus)
    endif ()
    if (TARGET ext_openh264)
        add_dependencies(steamview ext_openh264)
    endif ()

    install(TARGETS steamview RUNTIME DESTINATION bin)
    # runtime libs to ship in the IPK (set by the SDL2 backport module)
    include(PackageWebOS)
else ()
    # ---- host build: OSD menus + decoded video in SDL window ----
    add_executable(steamview ${SOURCES})
    target_compile_definitions(steamview PRIVATE SDL_MAIN_HANDLED)
    target_include_directories(steamview PRIVATE src)
    target_include_directories(steamview SYSTEM PRIVATE ${OPENH264_INCLUDE_DIRS})
    if (TARGET SDL2::SDL2net)
        target_link_libraries(steamview PRIVATE SDL2::SDL2 ihslib ${OPENH264_LIBRARIES} stdc++ SDL2::SDL2net ${MBEDCRYPTO_LIBRARY})
    else ()
        target_link_libraries(steamview PRIVATE SDL2::SDL2 ihslib ${OPENH264_LIBRARIES} stdc++ ${MBEDCRYPTO_LIBRARY})
    endif ()
    if (TARGET ext_openh264)
        add_dependencies(steamview ext_openh264)
    endif ()
    if (WIN32)
        target_link_libraries(steamview PRIVATE ws2_32)
        # place runtime DLLs beside the exe after build
        add_custom_command(TARGET steamview POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/third_party/SDL-webOS/build/SDL2.dll" $<TARGET_FILE_DIR:steamview>
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/third_party/SDL_net/build/SDL2_net.dll" $<TARGET_FILE_DIR:steamview>)
        add_custom_command(TARGET steamview POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_BINARY_DIR}/ext_mbedtls-prefix/bin/libmbedcrypto.dll" $<TARGET_FILE_DIR:steamview>)
    endif ()
endif ()

if (STEAMVIEW_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
