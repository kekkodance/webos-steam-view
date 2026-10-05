# protobuf-c runtime, vendored from the submodule.
# Layouts differ per branch; normalize into third_party/protobuf-c-out so
# <protobuf-c/protobuf-c.h> resolves the way ihslib's .pb-c.h files expect.
# (Linux hosts with libprotobuf-c-dev: system package wins via the variable
# being pre-set by the caller, or ihslib's pkg-config path.)

if (PROTOBUF_C_FOUND)
    return()
endif ()

find_package(PkgConfig QUIET)
if (PkgConfig_FOUND)
    pkg_check_modules(SYS_PROTOBUF_C QUIET libprotobuf-c)
    if (SYS_PROTOBUF_C_FOUND)
        set(PROTOBUF_C_FOUND TRUE)
        set(PROTOBUF_C_INCLUDEDIR ${SYS_PROTOBUF_C_INCLUDEDIR})
        set(PROTOBUF_C_LIBRARIES ${SYS_PROTOBUF_C_LIBRARIES})
        return()
    endif ()
endif ()

set(_PB_OUT ${CMAKE_BINARY_DIR}/protobuf-c-out)
file(MAKE_DIRECTORY ${_PB_OUT}/protobuf-c)
configure_file(third_party/protobuf-c/protobuf-c/protobuf-c.h ${_PB_OUT}/protobuf-c/protobuf-c.h COPYONLY)

add_library(protobuf-c STATIC third_party/protobuf-c/protobuf-c/protobuf-c.c)
target_include_directories(protobuf-c PUBLIC ${_PB_OUT})
set(PROTOBUF_C_FOUND TRUE)
set(PROTOBUF_C_INCLUDEDIR ${_PB_OUT})
set(PROTOBUF_C_LIBRARIES protobuf-c)
