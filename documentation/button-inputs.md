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

[describe in more details the steps require to connect and authenticate a client]

# Button mapping

[describe in more details the different commands that can be sent A B X Y, D-Pad inputs, select, start, R, L]
