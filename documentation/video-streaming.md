# Streaming the bottom screen output

To send the output of the bottom screen to the client, the host uses a websocket connection.
30 times per second, a jpeg (256\*192px) of the bottom screen is parsed and sent to the client. The client can then decode every picture as they arrive. A connection drop or a missing jpeg will not block the stream.

## Websocket parsing

Server \u2192 client: binary WebSocket frame

| Bytes | Content |
| --- | --- |
| `0-3` | `WMF2` |
| `4-7` | frame sequence, `uint32` little-endian |
| `8-15` | capture timestamp in microseconds, `uint64` little-endian |
| `16-17` | width, `uint16` little-endian: `256` |
| `18-19` | height, `uint16` little-endian: `192` |
| `20` | image format: `1` (JPEG) |
| `21-23` | `0x00 0x00 0x00` |
| `24+` | JPEG payload |

## Client reply

Client \u2192 server: text WebSocket frame

```json
{"v":2,"type":"frameAck","seq":FRAME_SEQUENCE,"decodeMs":DECODE_TIME_MS}
```

If JPEG decoding fails:

```json
{"v":2,"type":"frameAck","seq":FRAME_SEQUENCE}
```
