# OpenH264 for menus (encode) + host video (decode). Meson build (upstream
# has no CMake). Vendored tarball: third_party/openh264-2.4.1.tar.gz.
if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

include(ExternalProject)

set(LIB_FILENAME "${CMAKE_STATIC_LIBRARY_PREFIX}openh264${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(_install_dir "${CMAKE_BINARY_DIR}/openh264-install")

string(TOLOWER "${CMAKE_BUILD_TYPE}" _oh_buildtype)
if (_oh_buildtype STREQUAL "")
    set(_oh_buildtype "release")
endif ()

# Meson cross file for webOS (native build needs none).
set(_oh_cross "")
if (CMAKE_TOOLCHAIN_FILE)
    set(_oh_cross_file "${CMAKE_BINARY_DIR}/openh264-cross.ini")
    file(WRITE "${_oh_cross_file}" "[binaries]\nc = '${CMAKE_C_COMPILER}'\ncpp = '${CMAKE_CXX_COMPILER}'\nar = '${CMAKE_AR}'\n")
    set(_oh_cross "--cross-file" "${_oh_cross_file}")
endif ()

ExternalProject_Add(ext_openh264
        URL "${CMAKE_SOURCE_DIR}/third_party/openh264-2.4.1.tar.gz"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        CONFIGURE_COMMAND meson setup <BINARY_DIR> <SOURCE_DIR>
            --prefix=<INSTALL_DIR> --libdir=lib --buildtype=${_oh_buildtype}
            --default-library=static ${_oh_cross}
        BUILD_COMMAND ninja -C <BINARY_DIR>
        INSTALL_COMMAND ninja -C <BINARY_DIR> install
        BUILD_BYPRODUCTS <INSTALL_DIR>/lib/${LIB_FILENAME}
        INSTALL_DIR "${_install_dir}"
        )
ExternalProject_Get_Property(ext_openh264 INSTALL_DIR)

# --libdir=lib above keeps the archive at a fixed path (no multiarch dir).
add_library(ext_openh264_target STATIC IMPORTED)
set_target_properties(ext_openh264_target PROPERTIES IMPORTED_LOCATION ${INSTALL_DIR}/lib/${LIB_FILENAME})
add_dependencies(ext_openh264_target ext_openh264)

set(OPENH264_INCLUDE_DIRS ${INSTALL_DIR}/include)
set(OPENH264_LIBRARIES ext_openh264_target)
set(OPENH264_FOUND TRUE)
