# Wide Melon Linux Client

> [!WARNING]
> **USE AT YOUR OWN RISK**
> This software is currently in early development and untested, it can potentially damage your console
> If you want to compile and run this app, be aware of the risks.

Wide Melon Linux Client is an ARM64 Linux app that lets you play NDS games on the [WideMelon DS](https://github.com/pruefsumme/widemelon) emulator, using your linux handheld as a controller.

## Download the app

[Check the releases](https://github.com/NiCo-dev123/widemelon-linux-client/releases) and download the latest on your computer.
Extract the .zip file, copy the folder on your SD card in `/Apps/`

> [!NOTE]
> This project is only compatible with the Trimui Smart Pro S and SpruceOS.
> In the future, compatibility will be extended.

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
export TRIMUI_TSPS_SDK=/PATH_TO_PROJECT/.toolchains/trimui-smartpro-s/sdk_tg5050_linux_v1.0.0
cmake -S . -B build-tsps -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/TrimuiSmartProS.cmake
cmake --build build-tsps
```
