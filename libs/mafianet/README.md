# MafiaNet in ofxRakNet (`mafianet` branch)

Source: [MafiaHub/MafiaNet](https://github.com/MafiaHub/MafiaNet) `master` at `0e67de9` (2026-10-18,
after v0.23.1). MafiaNet is an actively maintained fork of RakNet 4 and
[SLikeNet](https://github.com/SLikeSoft/SLikeNet): C++17, OpenSSL 3+, fuzzing and sanitizers in CI, and
the same `RakPeerInterface` API in `namespace MafiaNet`, with snake_case headers (`mafianet/peer_interface.h`)
and `enum class` `Reliability` / `Priority`.

It already fixes what ofxRakNet's `master` patches into SLikeNet:

- **Split-message reassembly** (SLikeNet issue #82): fragments are kept in a map (no allocation sized
  from a forged count) and `SplitPacketSort::Add` rejects any fragment whose id, count or index doesn't
  match its message, or a non-final fragment ending mid-byte.
- **Congestion-control undefined behaviour** (the `UNSET_TIME_US` cast that clang turned into a trap).
- **Crypto on OpenSSL 1.1+.**

Included: the core library, `Source/include/mafianet` and `Source/src`, without `voice.cpp` (RakVoice
needs Opus and RNNoise; its header stays, unused). Left out: samples, extensions, tests, benchmarks and
the CMake files.

Licenses (`license/`): `LICENSE.md` (MafiaNet, MIT), and, because the sources still carry their
headers, `license.txt` (SLikeNet, MIT), `licenses/RakNet License.txt` (RakNet, BSD) and
`RakNet Patents.txt`, from SLikeNet.

The patches below apply to MafiaNet's tree as it is (`git apply`, from its root):

```sh
cd MafiaNet && git apply ofxRakNet/libs/mafianet/patches/*.patch
```

## 0001: TLS on (configuration)

`define_overrides.h`, the user-editable settings file, sets `OPEN_SSL_CLIENT_SUPPORT 1`, so
`TCPInterface` (`StartSSLClient`), `HTTPConnection`, `HTTPConnection2` and `Rackspace` can use TLS
through openFrameworks' OpenSSL. Define `OPEN_SSL_CLIENT_SUPPORT 0` before including MafiaNet to build
without it.

## 0002: TLS client builds (`const SSL_METHOD`)

With TLS on, `tcp_interface.cpp` didn't compile: `TLS_client_method()` returns `const SSL_METHOD *`
(since OpenSSL 1.1), assigned to a non-const member. The member and `RemoteClient::InitSSL` take
`const SSL_METHOD *`. MafiaNet builds with TLS off by default, so it doesn't hit this.
