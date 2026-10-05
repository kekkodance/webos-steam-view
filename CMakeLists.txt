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
        src/ui/ui.c
        src/ui/ui_port.c
        assets/fonts/motiva_20.c
        assets/fonts/motiva_28.c
        assets/fonts/motiva_40.c
        assets/fonts/motiva_medium_28.c
        assets/fonts/motiva_bold_28.c
        assets/fonts/motiva_pin_96.c)

if (TARGET_WEBOS)
    list(APPEND SOURCES src/webos/media.c)
else ()
    list(APPEND SOURCES tests/host_stubs.c)
endif ()

# ---- LVGL v9 (Steam-TV UI; software render, no EGL/Wayland) ----
# lv_conf.h lives in src/ui; LV_CONF_INCLUDE_SIMPLE picks it up.
set(LV_CONF_INCLUDE_SIMPLE ON CACHE BOOL "" FORCE)
set(LV_CONF_DIR ${CMAKE_SOURCE_DIR}/src/ui CACHE PATH "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
if (TARGET_WEBOS)
    # SDL2 backport for webOS (webosbrew build, like moonlight-tv ships);
    # must resolve before find_package(SDL2) so it takes precedence.
    # It also must come before add_subdirectory(lvgl): LVGL's SDL driver
    # needs SDL2::SDL2's includes on its own compile lines.
    set(SDL2_BACKPORT_RELEASE "release-2.30.12-webos.6")
    include(ExternalSDL2BackportForWebOS)
endif ()

find_package(SDL2 REQUIRED)
# (FindSDL2 also creates SDL2::SDL2net from third_party/SDL_net when present)
add_subdirectory(third_party/lvgl)
target_link_libraries(lvgl PUBLIC SDL2::SDL2)

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
    find_package(OPUS)
    if (NOT OPUS_FOUND)
        include(ExternalOPUS)
    endif ()

    add_executable(steamview ${SOURCES})
    target_include_directories(steamview PRIVATE src src/lgnc/include ${UI_INCLUDE_DIRS})
    target_compile_definitions(steamview PRIVATE APPID="${WEBOS_APPINFO_ID}" TARGET_WEBOS)
    target_link_libraries(steamview PRIVATE SDL2::SDL2 ihslib lvgl ${LGNCOPENAPI_LIBRARIES} ${OPUS_LIBRARIES} dl)
    target_include_directories(steamview SYSTEM PRIVATE ${LGNCOPENAPI_INCLUDE_DIRS} ${OPUS_INCLUDE_DIRS})

    install(TARGETS steamview RUNTIME DESTINATION bin)
    # runtime libs to ship in the IPK (set by the SDL2 backport module)
    include(PackageWebOS)
else ()
    # ---- host build (UI/protocol smoke tests; media stubbed) ----
    add_executable(steamview ${SOURCES})
    target_compile_definitions(steamview PRIVATE SDL_MAIN_HANDLED)
    target_include_directories(steamview PRIVATE src ${UI_INCLUDE_DIRS})
    if (TARGET SDL2::SDL2net)
        target_link_libraries(steamview PRIVATE SDL2::SDL2 ihslib lvgl SDL2::SDL2net ${MBEDCRYPTO_LIBRARY})
    else ()
        target_link_libraries(steamview PRIVATE SDL2::SDL2 ihslib lvgl ${MBEDCRYPTO_LIBRARY})
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
