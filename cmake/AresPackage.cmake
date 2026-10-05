# Pack staged install tree into a webOS IPK.
#
# Tries ares-package (webOS SDK CLI) first; when unavailable (plain CI
# runners), builds the same ipk manually with ar + tar. Layout mirrors what
# ares-package produces and what webOS accepts (same structure moonlight-tv
# ships): data.tar.gz with ./usr/palm/applications/<id>/..., debian-binary 2.0.

find_program(ARES_PACKAGE ares-package)
if (ARES_PACKAGE)
    execute_process(COMMAND ares-package "${CPACK_TEMPORARY_DIRECTORY}" -o "${CPACK_PACKAGE_DIRECTORY}"
            --force-arch "${CPACK_WEBOS_PACKAGE_ARCH}"
            COMMAND_ERROR_IS_FATAL ANY)
    return()
endif ()

message(STATUS "ares-package not found; packing IPK with ar/tar")

set(_PKGDIR "${CPACK_PACKAGE_DIRECTORY}/ipk-staging")
set(_PKG "${CPACK_PACKAGE_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}.ipk")
file(REMOVE_RECURSE "${_PKGDIR}")
file(MAKE_DIRECTORY "${_PKGDIR}/control" "${_PKGDIR}/data")

# data.tar.gz: staged install tree rooted at ./
file(COPY "${CPACK_TEMPORARY_DIRECTORY}/" DESTINATION "${_PKGDIR}/data")
file(CHMOD "${_PKGDIR}/data/usr/palm/applications/${CPACK_PACKAGE_NAME}/bin/steamview" PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

file(WRITE "${_PKGDIR}/control/control" "Package: ${CPACK_PACKAGE_NAME}
Version: ${CPACK_PACKAGE_VERSION}
Section: misc
Priority: optional
Architecture: ${CPACK_WEBOS_PACKAGE_ARCH}
Installed-Size: 10240
Maintainer: N/A <nobody@example.com>
Description: This is a webOS application.
webOS-Package-Format-Version: 2
webOS-Packager-Version: ci
")
file(WRITE "${_PKGDIR}/debian-binary" "2.0")

file(REMOVE "${_PKG}")
execute_process(COMMAND ${CMAKE_COMMAND} -E tar czf "${_PKGDIR}/data.tar.gz" ./usr
        WORKING_DIRECTORY "${_PKGDIR}/data" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND ${CMAKE_COMMAND} -E tar czf "${_PKGDIR}/control.tar.gz" ./control
        WORKING_DIRECTORY "${_PKGDIR}/control" COMMAND_ERROR_IS_FATAL ANY)

# ar archive: debian-binary, control.tar.gz, data.tar.gz (must be that order)
# ipk = ar archive of: debian-binary, control.tar.gz, data.tar.gz (in that order)
execute_process(COMMAND ${CMAKE_AR} -rc "${_PKG}" "debian-binary" "control.tar.gz" "data.tar.gz"
        WORKING_DIRECTORY "${_PKGDIR}" COMMAND_ERROR_IS_FATAL ANY)
file(REMOVE_RECURSE "${_PKGDIR}")
message(STATUS "IPK written to ${_PKG}")
