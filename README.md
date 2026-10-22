# ofxRakNet

[![ofxRakNet](https://github.com/danoli3/ofxRakNet/actions/workflows/ci.yml/badge.svg)](https://github.com/danoli3/ofxRakNet/actions/workflows/ci.yml)

RakNet networking for openFrameworks: reliable UDP for multiplayer, installations and anything else
that needs to talk between machines, with connection events and message delivery on the main thread.

It's built on [SLikeNet](https://github.com/SLikeSoft/SLikeNet), the maintained fork of RakNet 4
(RakNet itself, by Oculus / Facebook, is archived). SLikeNet's core is included as source in
`libs/slikenet`, with fixes for remote memory-safety bugs in large-message reassembly and a crash on
current compilers, and its OpenSSL parts (TLS for `TCPInterface` / HTTP, and the `crypto` helpers)
ported to the OpenSSL openFrameworks ships: see [libs/slikenet/PATCHES.md](libs/slikenet/PATCHES.md).

## Quick start

```cpp
#include "ofxRakNet.h"

ofxRakNet::Peer peer;

// setup: a server...
peer.startServer(60000);
// ...or a client
peer.connect("192.168.1.10", 60000);

peer.onConnect.newListener([](ofxRakNet::ConnectionEvent & e) { ofLog() << "connected " << e.address; });
peer.onMessage.newListener([](ofxRakNet::MessageEvent & e) {
	if (e.type == 0) ofLog() << e.getText();
	if (e.type == 1) ofLog() << e.getJson().dump();
});

// update: deliver what arrived (events fire here, on the main thread)
peer.update();

// send to one peer, or to everyone
peer.send(otherPeerId, 0, std::string("hello"));   // ids come from onConnect / onMessage
peer.broadcast(1, ofJson { { "x", 10 }, { "y", 20 } });
peer.broadcast(2, &position, sizeof(position), 0, ofxRakNet::Reliability::UnreliableSequenced);
```

## Peer

| | |
|---|---|
| `startServer(port, maxConnections = 32, password = "")` | listen for clients (a server can also `connect`) |
| `startClient()` / `connect(host, port, password = "")` | connect to a server (`connect` starts the client if needed) |
| `disconnect(peer)`, `stop()` | close one connection, or everything (also on destruction) |
| `send(peer, type, data, reliability, priority, channel)` | `std::string`, `ofBuffer`, `ofJson`, or a pointer and size |
| `broadcast(type, data, except = 0, ...)` | to every connected peer, except one |
| `update()` / `setAutoUpdate(true)` | handle what arrived and notify the events |
| `getConnections()`, `getNumConnections()`, `getAddress(peer)`, `getPing(peer)`, `getMyId()`, `getPort()` | |
| `getRakPeer()` | the `SLNet::RakPeerInterface`, for the rest of RakNet (plugins, statistics, NAT punch-through, ...) |

Events: `onConnect` (`ConnectionEvent`), `onDisconnect` (`DisconnectionEvent` with a `Reason`:
disconnected, lost, connect failed, server full, invalid password, banned, incompatible protocol,
already connected), `onMessage` (`MessageEvent`: `peer`, `address`, `type`, `data`, `getText()`,
`getJson()`), and `onPacket` for any other RakNet packet.

Message `type` is yours: 0 to `Peer::MAX_MESSAGE_TYPE` (121), one byte on the wire.

`Reliability`: `Unreliable`, `UnreliableSequenced` (positions, cursors: late ones are dropped),
`Reliable`, `ReliableOrdered` (default: chat, state), `ReliableSequenced`. `Priority`: `Immediate`,
`High` (default), `Medium`, `Low`. Large messages are split and reassembled for you (up to about 90 MB,
see PATCHES.md).

## Examples

Run two copies of an example: the first becomes the server on the port in `bin/data/settings.json`,
the next find the port taken and connect to it. To connect across machines, set `"host"` there.

| Example | |
|---|---|
| `example_cursors` | every copy's mouse cursor in every window (unreliable sequenced, relayed by the server) |
| `example_chat` | type and press Enter: JSON lines to everyone (reliable ordered) |
| `example_tests` | the addon's tests, headless: servers and clients on localhost, text, JSON, a 1 MB message, broadcast, passwords, full servers, the split-packet patch, and AES encryption through OpenSSL (49 checks) |

Build them with projectGenerator, or copy `scripts/templates/<platform>/Makefile` and `config.make`
into the example folder.

## Platforms

Tested on macOS with the openFrameworks makefiles, projectGenerator / Xcode, and AddressSanitizer,
and in CI on Linux and macOS. SLikeNet also supports Windows (Visual Studio, MSYS2), iOS and Android;
`ws2_32` is linked on Windows from `ofxRakNet.cpp`. OpenSSL comes from openFrameworks
(`libs/openssl`, or the system's on Linux), so the addon needs nothing extra.

## License

ofxRakNet: MIT, Dan Rosser. SLikeNet: MIT, SLikeSoft. RakNet: BSD, Oculus VR (see
`libs/slikenet/license`).

```
BSD License
For RakNet software

Copyright (c) 2014, Oculus VR, Inc.
All rights reserved.
```

(full text in `libs/slikenet/license/licenses/RakNet License.txt`, patent grant in `RakNet Patents.txt`)
