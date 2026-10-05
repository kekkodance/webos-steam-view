# webOS IPK packaging for Steam View.
#
# Produces com.kekko.steamview_<version>_arm.ipk with:
#   /usr/palm/applications/com.kekko.steamview/bin/steamview
#   /usr/palm/applications/com.kekko.steamview/lib/*.so   (SDL2 backport, mbedcrypto)
#   /usr/palm/applications/com.kekko.steamview/appinfo.json + icons
#
# Cross-build requirements (same as moonlight-tv):
#   - webOS NDK: openlgtv/buildroot-nc4 toolchain (arm-webos-linux-gnueabi)
#     TOOLCHAIN_FILE=<ndk>/share/buildroot/toolchainfile.cmake
#   - webos-userland headers via pkg-config (lgncopenapi, opus)
#   - ares-package (webOS SDK CLI) to pack the ipk, or the fallback tar path below

set(WEBOS_APPINFO_ID "com.kekko.steamview")
set(CPACK_PACKAGE_NAME "${WEBOS_APPINFO_ID}")
set(CPACK_WEBOS_PACKAGE_ARCH "arm")
set(CPACK_GENERATOR "External")
set(CMAKE_AR ar CACHE STRING "")
set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/AresPackage.cmake")
set(CPACK_EXTERNAL_ENABLE_STAGING TRUE)
set(CPACK_MONOLITHIC_INSTALL TRUE)
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_SOURCE_DIR}/dist)
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${PROJECT_VERSION}_arm")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/palm/applications/${WEBOS_APPINFO_ID}")

# appinfo + icons land at the app root (version stamped from the project)
configure_file(deploy/webos/appinfo.json "${CMAKE_CURRENT_BINARY_DIR}/appinfo.json" @ONLY)
install(DIRECTORY deploy/webos/ DESTINATION . USE_SOURCE_PERMISSIONS
        PATTERN "appinfo.json" EXCLUDE)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/appinfo.json" DESTINATION .)

# binaries: app in bin/, shared libs in lib/ with rpath $ORIGIN/../lib
if (NOT DEFINED CMAKE_INSTALL_BINDIR)
    set(CMAKE_INSTALL_BINDIR bin)
endif ()
if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR lib)
endif ()

# SDL2 backport and mbedcrypto ship alongside the app
foreach (RUNTIME_LIB ${SDL2_RUNTIME_LIBS} ${MBEDCRYPTO_RUNTIME})
    if (RUNTIME_LIB)
        install(FILES "${RUNTIME_LIB}" DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif ()
endforeach ()

add_custom_target(webos-package
        COMMAND cpack
        DEPENDS steamview
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Packing webOS IPK")

include(CPack)
