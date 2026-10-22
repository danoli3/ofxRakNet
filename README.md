# ofxRakNet

[![ofxRakNet](https://github.com/danoli3/ofxRakNet/actions/workflows/ci.yml/badge.svg)](https://github.com/danoli3/ofxRakNet/actions/workflows/ci.yml)

RakNet networking for openFrameworks: reliable UDP for multiplayer, installations and anything else
that needs to talk between machines, with connection events and message delivery on the main thread.

**This is the `mafianet` branch:** it's built on [MafiaNet](https://github.com/MafiaHub/MafiaNet), an
actively maintained fork of RakNet 4 and [SLikeNet](https://github.com/SLikeSoft/SLikeNet) (RakNet
itself, by Oculus / Facebook, is archived; SLikeNet's last change was in 2021). MafiaNet's core is
included as source in `libs/mafianet`, with TLS turned on for openFrameworks' OpenSSL: see
[libs/mafianet/README.md](libs/mafianet/README.md). `master` uses SLikeNet with ofxRakNet's own fixes,
which MafiaNet already has.

> **Consider [ofxENet](https://github.com/danoli3/ofxENet) once it's complete.** ofxENet has the same API (change `ofxRakNet` to `ofxENet`)
> on [ENet](https://github.com/zpl-c/enet), which is small and actively maintained. Stay on ofxRakNet if
> you need RakNet itself: talking to other RakNet apps, or its plugins (replica manager, RPC, NAT
> punch-through, TLS through TCPInterface).

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

## Encryption

```cpp
peer.setPassphrase("gallery-secret");   // every peer uses the same passphrase
peer.requireEncryption = true;          // optional: drop anything not encrypted with it
ofLog() << "key " << peer.getFingerprint();   // compare between machines

peer.onMessage.newListener([](ofxRakNet::MessageEvent & e) {
	if (e.encrypted && !e.authentic) return; // wrong passphrase, or changed on the way
	ofLog() << e.getText();
});
```

With a passphrase, every message's payload is encrypted with **AES-256-GCM** (OpenSSL), so its content
is hidden and any change on the way is detected. The key comes from the passphrase with
**PBKDF2-HMAC-SHA256** (200,000 iterations) and a random 16 byte **salt** per session. The salt travels
with each message, and receivers derive each sender's key once and cache it. Every message gets a fresh
random 12 byte nonce. `MessageEvent::encrypted` and `authentic` say what happened.
`ofxRakNet::crypto` (`ofxRakNetCrypto.h`) has the pieces on their own: `Cipher`, `sha256`,
`deriveKey`, `randomBytes`, `toHex`.

Limits: the message type byte and sizes aren't hidden; there's no forward secrecy (anyone who learns
the passphrase can read recorded traffic); a weak passphrase can be guessed offline, PBKDF2 only slows
that down; and a relaying server needs the passphrase to read and re-send.

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

Message `type` is yours: 0 to `Peer::MAX_MESSAGE_TYPE` (120), one byte on the wire.

`Reliability`: `Unreliable`, `UnreliableSequenced` (positions, cursors: late ones are dropped),
`Reliable`, `ReliableOrdered` (default: chat, state), `ReliableSequenced`. `Priority`: `Immediate`,
`High` (default), `Medium`, `Low`. Large messages are split and reassembled for you.

## Examples

Run two copies of an example: the first becomes the server on the port in `bin/data/settings.json`,
the next find the port taken and connect to it. To connect across machines, set `"host"` there.

| Example | |
|---|---|
| `example_cursors` | every copy's mouse cursor in every window (unreliable sequenced, relayed by the server) |
| `example_chat` | type and press Enter: JSON lines to everyone (reliable ordered) |
| `example_imgui_messenger` | needs [ofxImGui](https://github.com/jvcleave/ofxImGui) (`develop`): connection settings, a passphrase for encryption (with key fingerprint), a peer list with ping (pick who to send to, kick), compose text or JSON with any reliability / priority / channel, a chat view (name: text) and a filterable log with each message's SHA-256, and server relay with the original sender framed in. Start encrypted with `"passphrase"` in `settings.json` or `OFXRAKNET_PASSPHRASE` |
| `example_tests` | the addon's tests, headless: servers and clients on localhost, text, JSON, a 1 MB message, broadcast, passwords, full servers, split-packet checks, MafiaNet's AES helpers, and ofxRakNet's encryption (cipher, salts, tampering, wrong passphrases, encrypted peers) (69 checks) |

Build them with projectGenerator, or copy `scripts/templates/<platform>/Makefile` and `config.make`
into the example folder.

## Platforms

Tested on macOS with the openFrameworks makefiles, projectGenerator / Xcode, and AddressSanitizer,
and in CI on Linux and macOS. MafiaNet also supports Windows (Visual Studio, MSYS2), iOS and Android;
`ws2_32` is linked on Windows from `ofxRakNet.cpp`. OpenSSL comes from openFrameworks
(`libs/openssl`, or the system's on Linux), so the addon needs nothing extra.

## License

ofxRakNet: MIT, Dan Rosser. MafiaNet: MIT, MafiaHub. SLikeNet: MIT, SLikeSoft. RakNet: BSD, Oculus VR
(see `libs/mafianet/license`).

```
BSD License
For RakNet software

Copyright (c) 2014, Oculus VR, Inc.
All rights reserved.
```

(full text in `libs/mafianet/license/licenses/RakNet License.txt`, patent grant in `RakNet Patents.txt`)
