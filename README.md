<p align="center">
  <img src="./assets/icons/wmclient-logo.png" />
</p>

# Wide Melon Linux Client

> [!WARNING]
> **USE AT YOUR OWN RISK**
> This software is currently in early development and untested, it can potentially damage your console.

Wide Melon Linux Client is an ARM64 Linux app that lets you play NDS games on the [WideMelon DS](https://github.com/pruefsumme/widemelon) emulator, using your linux handheld as a controller.

## Download the app

[Check the releases](https://github.com/NiCo-dev123/widemelon-linux-client/releases) and download the latest version on your computer.
Extract the .zip file, copy the folder on your SD card in `/Apps/`

> [!NOTE]
> This project is only compatible with the Trimui Smart Pro S.
> It was only tested on SpruceOS. It should launch but some features might be broken or missing.
> In the future, compatibility should be extended.

## Building for your machine

To produce the TSPS executable, configure CMake with an ARM64 Linux toolchain
and a sysroot matching the console firmware. The build machine must provide the
ARM64 SDL2 headers and library.

With the official SDK extracted under `.toolchains/trimui-smartpro-s/`, run:

```sh
export TRIMUI_TSPS_SDK=/PATH_TO_PROJECT/.toolchains/trimui-smartpro-s/sdk_tg5050_linux_v1.0.0
cmake -S . -B build-tsps -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/TrimuiSmartProS.cmake
cmake --build build-tsps
```
