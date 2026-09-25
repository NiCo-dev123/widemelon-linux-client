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

## Building for TSPS

The project includes `tools/build-tsps.sh`, which configures CMake, builds the ARM64 client, and assembles a complete application folder ready to copy to the SD card.

Create your local configuration file once:

```sh
cp tools/build-tsps.conf.example tools/build-tsps.conf
```

Edit `tools/build-tsps.conf` and set `TRIMUI_TSPS_SDK` to the absolute path of your extracted SDK. This file is ignored by Git, so local paths are never committed.

Then build with:

```sh
tools/build-tsps.sh
```

The ready-to-copy application is generated at `dist/WideMelonClient/`. Copy this folder into `/Apps/` on the SD card.
