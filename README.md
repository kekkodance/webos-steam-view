<div align="center">
  <h1>
    Steam View
  </h1>
  <p>Watch any Steam Remote Play host on a rooted webOS 1-4 LG TV.</p>
  <img width="1280" height="720" alt="image" src="https://github.com/user-attachments/assets/ea3b28e3-244d-46ff-8019-80dafd205d88" />

  <p>I built it for my Steam Frame's Spectator View, but it works with a PC or anything
else running Steam.</p>
</div>

How it works:

- Protocol: [ihslib](https://github.com/mariotaku/ihsplay) (`plume-fixes`
  branch, which has session fixes master is still missing)
- Video: H.264 straight to the TV's hardware decoder (LGNC DirectVideo,
  same path moonlight-tv uses)
- Audio: Opus decoded to PCM, out over LGNC DirectAudio
- UI: LVGL on SDL2 (the [webosbrew backport](https://github.com/webosbrew/SDL-webOS))

Nothing gets installed on the host. The TV is just a Remote Play client.

## Why not IHSplay?

[ihsplay](https://github.com/mariotaku/ihsplay) is the obvious answer and
I tried it first. It crashes on startup on webOS 2
([#29](https://github.com/mariotaku/ihsplay/issues/29)): the display init
divides by a zero window size on the old Wayland stack. The protocol code
is fine, so this app reuses the same protocol library and just does its own
simpler display layer: fixed 1280x720 software UI surface, video on the LGNC
plane where it belongs.

## Trying it on your PC

The host build is the full app with the TV decoder swapped for stubs. Same
protocol, same UI, no picture (your PC has no LGNC decoder, so it just
counts frames). Good for testing pairing:

```sh
cmake -B build-host -DCMAKE_BUILD_TYPE=Release .
cmake --build build-host
./build-host/steamview
```

or just double-click `build-host/steamview.exe` on Windows.

1. It finds Steam hosts on your LAN (your own PC's Steam shows up).
2. Pick one, press Enter. A 4-digit PIN pops up.
3. Type it into the Steam dialog on the host.
4. Accept the Remote Play prompt. The status line starts counting
   video/audio frames.

Pairing sticks around in `.steamview-identity` next to the exe, so Steam
remembers the device.

Needs SDL2, SDL2_net, protobuf-c, mbedtls. CI fetches all of it.

## Building the IPK

You need the webOS NDK (one time):

```sh
export TOOLCHAIN_FILE=/path/to/arm-webos-linux-gnueabi_sdk-buildroot/share/buildroot/toolchainfile.cmake
./scripts/build-webos.sh
# -> dist/com.kekko.steamview_<version>_arm.ipk
```

Then sideload the IPK however you like: Homebrew Channel, Dev Manager, or
`ares-install dist/*.ipk`.

## Using it

1. Host and TV on the same network, host awake.
2. Open Steam View on the TV. It lists what it finds.
3. Pick one, press OK, type the PIN into Steam on the host.
4. Accept the prompt on the host. Done, its screen is on your TV.

## License

GPL-3.0-or-later (see LICENSE).
LVGL is MIT, SDL2 and SDL_net are zlib, mbedtls is Apache-2.0,
protobuf-c is BSD-2.
