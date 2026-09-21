# Sending button inputs

WideMelon DS uses the websocket protocol to receive button inputs from the client.
The client is first authenticated with a code and the connection is created.

1. Connect to `ws://HOST_IP:PORT/bridge
2. Use the correct HTTP origin during the handshake
3. Authenticate :

```
{"v":2,"type":"auth","credential":"SECRET_CODE"}
```

4. Send commands :

```
{
  "v": 2,
  "type": "input",
  "seq": 1,
  "buttons": 1,
  "hotkeys": 0,
  "touch": {"active": false, "x": 0, "y": 0}
}
```

# Websocket connection

The WebSocket server listens on the same host and TCP port as the HTTP server.
The client must be on the selected private subnet; WideMelon accepts one paired
client at a time.

1. Open a WebSocket connection to `ws://HOST_IP:PORT/bridge`.
2. Send an HTTP `Origin` header equal to `http://HOST_IP:PORT` during the
   WebSocket handshake. WideMelon rejects another origin.
3. Send this text WebSocket message after the socket opens:

   ```json
   {"v":2,"type":"auth","credential":"SECRET_CODE"}
   ```

   `SECRET_CODE` is either the session secret contained in the QR-code URL or
   the ten-digit pairing code displayed by WideMelon. It is regenerated when
   the server starts or when pairing is revoked.

4. Wait for the text `hello` response before sending input. It confirms that
   authentication succeeded and contains the active controller layout.

   ```json
   {"v":2,"type":"hello","width":256,"height":192,"fps":30,"layout":{...}}
   ```

5. Send `input` messages as text WebSocket frames. Each message is a complete
   input snapshot, not a press/release event. Increment `seq` for every input
   message, starting at `1` for each new connection.

The client should send the current snapshot whenever a button changes and at
least once every 200 ms while connected. WideMelon releases remote inputs after
one second without an input snapshot.

The server also sends `ping` messages. Reply with the supplied value:

```json
{"v":2,"type":"pong","sent":PING_SENT_VALUE}
```

The current bridge also streams binary JPEG frames. An input-only client must
read them and reply with `frameAck` using the frame sequence from byte offsets
4--7 of the little-endian frame header, otherwise WideMelon eventually closes
the stalled connection.

# Button mapping

`buttons` is an unsigned 12-bit bitmask. Set a bit to `1` when that DS button
is pressed; combine simultaneous presses with bitwise OR. Set `buttons` to `0`
to release every DS button.

| DS button | Bit | Decimal value | Hex value |
| --- | ---: | ---: | ---: |
| A | 0 | 1 | `0x001` |
| B | 1 | 2 | `0x002` |
| Select | 2 | 4 | `0x004` |
| Start | 3 | 8 | `0x008` |
| Right | 4 | 16 | `0x010` |
| Left | 5 | 32 | `0x020` |
| Up | 6 | 64 | `0x040` |
| Down | 7 | 128 | `0x080` |
| R | 8 | 256 | `0x100` |
| L | 9 | 512 | `0x200` |
| X | 10 | 1024 | `0x400` |
| Y | 11 | 2048 | `0x800` |

For example, A + Right is `buttons: 17` (`0x001 | 0x010`).

```json
{"v":2,"type":"input","seq":12,"buttons":17,"hotkeys":0,"touch":{"active":false,"x":0,"y":0}}
```

`hotkeys` is a separate bitmask for emulator actions and should be `0` for the
initial controller client. `touch` must always be present; leave it inactive
until touchscreen support is implemented. When used, `x` is from 0 to 255 and
`y` is from 0 to 191.
