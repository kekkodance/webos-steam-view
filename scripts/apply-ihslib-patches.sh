#!/usr/bin/env bash
# Apply the local fixes to third_party/ihslib (idempotent).
#
# Why: the upstream tree needs a handful of portability fixes for our builds:
#  - MSVC-strict compilers (MinGW gcc 16) reject implicit declarations that
#    older gcc tolerated: missing <string.h>/<math.h> includes
#  - the non-Unix SDL_net UDP backend lags the header API
#  - the SDL HID provider pulls SDL2/ headers we don't lay out locally
#
# Keep this script in sync with the CI workflow (it runs there before building).
set -euo pipefail
cd "$(dirname "$0")/.."
IHS=third_party/ihslib

if ! [ -d "$IHS" ]; then
    echo "ihslib not checked out; run: git submodule update --init" >&2
    exit 1
fi

python3 - << 'PYEOF'
import re, sys, pathlib

root = pathlib.Path("third_party/ihslib")

def patch(path, old, new, count=1):
    p = root / path
    s = p.read_text()
    if new in s:
        return False  # already applied
    if old not in s:
        print(f"PATTERN NOT FOUND in {path}: {old[:60]!r}", file=sys.stderr)
        sys.exit(1)
    p.write_text(s.replace(old, new, count))
    print(f"patched {path}")
    return True

# 1. base.c: strncpy needs <string.h>
patch("src/base.c",
      "#include <assert.h>\n",
      "#include <assert.h>\n#include <string.h>\n")

# 2. <memory.h> is not portable; the functions live in <string.h>
for f in root.rglob("*.c"):
    s = f.read_text()
    if "#include <memory.h>" in s:
        f.write_text(s.replace("#include <memory.h>", "#include <string.h>"))
        print(f"patched {f.relative_to(root)} (memory.h)")

# 3. ch_data_video.c: sqrt needs <math.h>
patch("src/session/channels/control/control_hid.c",
      '#include "control_hid.h"\n',
      '#include "control_hid.h"\n\n#include <stdlib.h>\n#include <string.h>\n')

patch("src/session/channels/video/ch_data_video.c",
      "#include <stdlib.h>\n",
      "#include <stdlib.h>\n#include <math.h>\n")

# 4. SDL_net UDP backend: align with ihs_udp.h API
patch("src/platforms/ihs_udp_sdl.c",
      "IHS_UDPSocket *IHS_UDPSocketOpen() {",
      "IHS_UDPSocket *IHS_UDPSocketOpen(bool broadcast) {\n    (void) broadcast;")
patch("src/platforms/ihs_udp_sdl.c",
      "int IHS_UDPSocketSend(IHS_UDPSocket *socket, IHS_UDPPacket *packet) {",
      "bool IHS_UDPSocketSend(IHS_UDPSocket *socket, const IHS_UDPPacket *packet) {")
patch("src/platforms/ihs_udp_sdl.c",
      "    return SDLNet_UDP_Send(socket->socket, -1, &sdlPacket);",
      "    return SDLNet_UDP_Send(socket->socket, -1, &sdlPacket) == 1;")

udp = root / "src/platforms/ihs_udp_sdl.c"
s = udp.read_text()
if "IHS_UDPSocketSetBlocking" not in s:
    s += '''
/* Local additions matching ihs_udp.h (mirrors ihs_udp_posix.c semantics).
 * SDL_net sockets are non-blocking by design; these are accepted as no-ops. */

bool IHS_UDPSocketSetBlocking(IHS_UDPSocket *s, bool blocking) {
    (void) s; (void) blocking;
    return true;
}

bool IHS_UDPSocketSetRecvTimeout(IHS_UDPSocket *s, uint32_t timeoutUs) {
    (void) s; (void) timeoutUs;
    return true;
}
'''
    udp.write_text(s)
    print("patched src/platforms/ihs_udp_sdl.c (SetBlocking/SetRecvTimeout)")

# 4a. platforms: link SDL via the target when it exists (fixes link order
# for static archives: ihs-platforms must carry the SDL dependency itself)
patch("src/platforms/CMakeLists.txt",
      "if (SDL2_FOUND)\n    target_sources(ihs-platforms PRIVATE ihs_thread_sdl.c)\n    target_include_directories(ihs-platforms PRIVATE SYSTEM ${SDL2_INCLUDE_DIRS})\n    target_link_libraries(ihs-platforms PUBLIC ${SDL2_LIBRARIES})\nendif ()",
      "if (SDL2_FOUND)\n    target_sources(ihs-platforms PRIVATE ihs_thread_sdl.c)\n    if (TARGET SDL2::SDL2)\n        target_link_libraries(ihs-platforms PUBLIC SDL2::SDL2)\n    endif ()\n    target_include_directories(ihs-platforms PRIVATE SYSTEM ${SDL2_INCLUDE_DIRS})\n    target_link_libraries(ihs-platforms PUBLIC ${SDL2_LIBRARIES})\nendif ()")
# (4a is a no-op when 6 already applied the TARGET SDL2::SDL2 variant)

# 4b2. 64KB SDL_net receive packet: a 2048-byte packet silently drops
# large discovery replies (the classic "no host answers" symptom)
patch("src/platforms/ihs_udp_sdl.c",
      "#include <time.h>\n",
      "/* Discovery replies can exceed 2048 bytes (hostname, app lists); Steam's\n * own packet size ceiling is 64KB. A too-small SDL_net packet silently drops\n * the whole datagram, which looks like 'no host answers'. */\n#define UDP_RECV_PACKET_SIZE 65535\n\n#include <time.h>\n")
patch("src/platforms/ihs_udp_sdl.c",
      "socket->packet = SDLNet_AllocPacket(2048);",
      "socket->packet = SDLNet_AllocPacket(UDP_RECV_PACKET_SIZE);")

# 4b. ihslib CMake: use our find modules instead of hard pkg-config
patch("CMakeLists.txt",
      """if (NOT PROTOBUF_C_FOUND)
    pkg_check_modules(PROTOBUF_C libprotobuf-c REQUIRED)
endif ()""",
      """if (NOT PROTOBUF_C_FOUND)
    find_package(protobuf-c)
endif ()

if (NOT PROTOBUF_C_FOUND)
    message(FATAL_ERROR "protobuf-c not found")
endif ()""")

# 4c. restore forward declarations (patch churn can delete them)
patch("src/platforms/ihs_udp_sdl.c",
      "};\n\n/* Discovery replies",
      "};\n\nstatic void AddressFromSDL(IHS_SocketAddress *ihs, const IPaddress *sdl);\nstatic void AddressToSDL(const IHS_SocketAddress *ihs, IPaddress *sdl);\n\n/* Discovery replies")

# 4d. worker receive buffer caps at 2048 bytes; UDP datagrams max out at
# 64KB. A capped copy fails silently (returns 0) and the packet is dropped,
# which looks like "no host answers" whenever a host's reply exceeds 2048.
patch("src/base.c",
      "IHS_BufferInit(&recv.buffer, 2048, 2048);",
      "IHS_BufferInit(&recv.buffer, 2048, 65535);")

# 4e. authorization ticket: Valve rejects tickets missing device_model,
# device_serial and device_provisioning_id (host answers AuthorizationFailed
# even for the correct PIN). The upstream steamlink.py RE client always sent
# these; ihslib dropped them. Values are informational only.
patch("src/client/authorization.c",
      "    ticket->device_name = client->base.deviceName;\n}",
      "    ticket->device_name = client->base.deviceName;\n    ticket->device_model = \"1234\";\n    ticket->device_serial = \"A1B2C3D4E5\";\n    ticket->has_device_provisioning_id = true;\n    ticket->device_provisioning_id = 123456;\n}")
# NOTE: src/base.c (per-host secret-key rotation: IHS_BaseSetSecretKey /
# IHS_BaseGetSecretKey) and src/client/client.c (IHS_ClientGet/SetSecretKey)
# come from scaronni/ihslib's key-exchange fix, now committed in our ihslib
# checkout. Do NOT re-apply old local edits there; the patch() calls above
# only touch the receive buffer and the ticket fields.

# 5. WinSock IP helpers (only built on WIN32 host builds)
ws = root / "src/platforms/ihs_ip_winsock.c"
if not ws.exists():
    ws.write_text('''/*
 * IP address helpers portable across POSIX and WinSock.
 * (ihslib only ships ihs_ip_posix.c; on Windows hosts we provide this.)
 */
#include "ihslib/net.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

char *IHS_IPAddressToString(const IHS_IPAddress *address) {
    assert(address->family == IHS_IPAddressFamilyIPv4 || address->family == IHS_IPAddressFamilyIPv6);
    if (address->family == IHS_IPAddressFamilyIPv6) {
        char buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, address->v6.data, buf, INET6_ADDRSTRLEN);
        return strdup(buf);
    } else {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, address->v4.data, buf, INET_ADDRSTRLEN);
        return strdup(buf);
    }
}

bool IHS_IPAddressFromString(IHS_IPAddress *address, const char *str) {
    if (inet_pton(AF_INET, str, address->v4.data) == 1) {
        address->v4.family = IHS_IPAddressFamilyIPv4;
        return true;
    }
    if (inet_pton(AF_INET6, str, address->v6.data) == 1) {
        address->v6.family = IHS_IPAddressFamilyIPv6;
        return true;
    }
    return false;
}
''')
    print("added src/platforms/ihs_ip_winsock.c")

# 6. platforms/CMakeLists.txt: winsock IP file, ws2_32, SDL2net target reuse
p = root / "src/platforms/CMakeLists.txt"
s = p.read_text()
if "ihs_ip_winsock.c" not in s:
    s = s.replace('''else ()
    pkg_check_modules(SDL2_NET SDL2_net REQUIRED)
    target_sources(ihs-platforms PRIVATE ihs_udp_sdl.c)
    target_include_directories(ihs-platforms PRIVATE SYSTEM ${SDL2_NET_INCLUDE_DIRS})
    target_link_libraries(ihs-platforms PUBLIC ${SDL2_NET_LIBRARIES})
endif ()''', '''else ()
    target_sources(ihs-platforms PRIVATE ihs_ip_winsock.c)
    if (WIN32)
        # ws2_32: sockets; iphlpapi: GetAdaptersAddresses for directed-broadcast discovery
        target_link_libraries(ihs-platforms PUBLIC ws2_32 iphlpapi)
    endif ()
    if (TARGET SDL2::SDL2net)
        set(SDL2_NET_LINK SDL2::SDL2net)
        set(SDL2_NET_INCLUDE_DIRS "")
    else ()
        pkg_check_modules(SDL2_NET SDL2_net REQUIRED)
        set(SDL2_NET_LINK ${SDL2_NET_LIBRARIES})
    endif ()
    target_sources(ihs-platforms PRIVATE ihs_udp_sdl.c)
    target_include_directories(ihs-platforms PRIVATE SYSTEM ${SDL2_NET_INCLUDE_DIRS})
    target_link_libraries(ihs-platforms PUBLIC ${SDL2_NET_LINK})
endif ()''')
    p.write_text(s)
    print("patched src/platforms/CMakeLists.txt")

# 7. gate the SDL HID provider behind an option (needs SDL2/ include layout)
p = root / "CMakeLists.txt"
s = p.read_text()
if 'option(IHS_HID_SDL' not in s:
    s = s.replace('option(IHSLIB_SANITIZE_THREAD "Link Thread Sanitizer (mutually exclusive with address)" OFF)',
                  'option(IHSLIB_SANITIZE_THREAD "Link Thread Sanitizer (mutually exclusive with address)" OFF)\n'
                  'option(IHS_HID_SDL "Build SDL HID provider" OFF)')
    p.write_text(s)
    print("patched CMakeLists.txt (IHS_HID_SDL option)")

h = root / "src/hid/CMakeLists.txt"
s = h.read_text()
if "if (IHS_HID_SDL)" not in s:
    s = s.replace("add_subdirectory(sdl)",
                  "if (IHS_HID_SDL)\n    add_subdirectory(sdl)\nendif ()")
    h.write_text(s)
    print("patched src/hid/CMakeLists.txt")

print("ihslib patches applied")
PYEOF
