# Touchscreen input

After the WebSocket handshake and the `hello` response, send a complete `input`
snapshot whenever the stylus moves, is pressed, or is released.

```json
{"v":2,"type":"input","seq":12,"buttons":0,"hotkeys":0,"touch":{"active":true,"x":128,"y":96}}
```

- `touch.active`: `true` while the stylus is pressed; `false` to release it.
- `touch.x`: integer from `0` to `255`.
- `touch.y`: integer from `0` to `191`.
- `seq`: increment for every snapshot, including stylus movements.

To move the cursor without touching the screen, keep `active` set to `false`
and provide its current `x` and `y` coordinates.

```json
{"v":2,"type":"input","seq":13,"buttons":0,"hotkeys":0,"touch":{"active":false,"x":128,"y":96}}
```
