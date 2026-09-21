# Wide Melon Linux Client

Wide Melon Linux Client is an ARM64 Linux app that lets you play NDS games on the [WideMelon DS](https://github.com/pruefsumme/widemelon) emulator, using your linux handheld as a controller.

## Configuration

Copy `config/widemelon-client.conf.example` next to the `widemelon-client`
executable and rename it to `widemelon-client.conf`. Set `host` to the private
IPv4 address displayed by WideMelon and set `pairing_code` to its current
ten-digit pairing code. `port` defaults to `24872` when omitted.
