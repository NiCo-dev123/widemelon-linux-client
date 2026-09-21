# Wide Melon Linux Client

**Status : Early development, not ready for testing**

Wide Melon Linux Client is an ARM64 Linux app that lets you play NDS games on the [WideMelon DS](https://github.com/pruefsumme/widemelon) emulator, using your linux handheld as a controller.

## Configuration

Copy `config/widemelon-client.conf.example` next to the `widemelon-client`
executable and rename it to `widemelon-client.conf`. Set `host` to the private
IPv4 address displayed by WideMelon and set `pairing_code` to its current
ten-digit pairing code. `port` defaults to `24872` when omitted.

## Trimui Smart Pro S / SpruceOS installation

Create this directory on the SD card:

```text
/mnt/SDCARD/App/WideMelonClient/
├── config.json
├── launch.sh
├── widemelon-client
└── widemelon-client.conf
```

Copy `config.json` and `launch.sh` from
`packaging/spruce/WideMelonClient/`. Copy the ARM64 `widemelon-client` binary
to the same directory. Copy `config/widemelon-client.conf.example` there as
`widemelon-client.conf`, then edit it with the private IPv4 address and current
pairing code shown by WideMelon.

Restart SpruceOS (or return to its main menu) after copying the folder. The
**WideMelon Client** entry should appear in the Apps menu because its
`config.json` declares `TRIMUI_SMART_PRO_S`. Launch it to display the loaded
host, port, and pairing code. Press any key or controller button to close the
smoke-test screen.

If the app returns immediately, inspect:

```text
/mnt/SDCARD/App/WideMelonClient/widemelon-client.log
```

If the ARM64 build needs a non-system SDL2 library, place it and its required
dependencies in `/mnt/SDCARD/App/WideMelonClient/lib/`; `launch.sh` adds that
directory to `LD_LIBRARY_PATH` for this app only.

## Building on a development machine

For a local host build and tests:

```sh
cmake -S . -B build -G "Unix Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

To produce the TSPS executable, configure CMake with an ARM64 Linux toolchain
and a sysroot matching the console firmware. The build machine must provide the
ARM64 SDL2 headers and library; do not use the host x86_64 SDL2 library.

With the official SDK extracted under `.toolchains/trimui-smartpro-s/`, run:

```sh
export TRIMUI_TSPS_SDK=/home/nicolas/Documents/wide-melon-ds/.toolchains/trimui-smartpro-s/sdk_tg5050_linux_v1.0.0
cmake -S . -B build-tsps -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/TrimuiSmartProS.cmake
cmake --build build-tsps
```
