# SLikeNet in ofxRakNet (`sabrosa` branch)

Source: [Sabrosa/slikenet](https://gitlab.com/Sabrosa/slikenet) `master` at `2413cc8b` (2026-09-19), a
maintained fork of [SLikeSoft/SLikeNet](https://github.com/SLikeSoft/SLikeNet) (last commit 2021),
itself the maintained fork of RakNet 4.082 (Oculus / Facebook, archived). Sabrosa's changes since
SLikeNet include IPv6 on by default with dual-stack sockets, OpenSSL 1.1+ only (the crypto port
ofxRakNet's `master` patches in), the congestion-control undefined behaviour fixed, a 4 MB cap on split
messages, `std::atomic` instead of `LocklessTypes`, and ARM, MinGW and Meson / CMake fixes.

Included: the core library, `Source/include/slikenet` and `Source/src` (without `meson.build`), with
the `crypto` helpers and TLS client support built against openFrameworks' OpenSSL. Left out: samples,
dependent extensions and bindings.

Licenses: `license/license.txt` (SLikeNet, MIT), `license/licenses/RakNet License.txt` (RakNet, BSD)
and `RakNet Patents.txt`, as listed in `_quick_licensing_slikenet_core.txt`.

The patches below apply to Sabrosa's tree as it is (`git apply`, from its root):

```sh
cd slikenet && git apply ofxRakNet/libs/slikenet/patches/*.patch
```

## 0001: split-packet reassembly bounds (SLikeNet issue #82)

[SLikeNet issue #82](https://github.com/SLikeSoft/SLikeNet/issues/82) (2026-08-24, unanswered) reports
three remote memory-safety defects in reassembling messages split over several datagrams
(`Source/src/ReliabilityLayer.cpp`, the default `SplitPacketSort` path). Any peer, connected or not, can
send the fragments.

1. **Out-of-bounds write.** `SplitPacketSort::Add` indexed its array with the fragment's
   `splitPacketIndex`, checked only by `RakAssert`, which is empty in release builds. The array is sized
   from the first fragment of a message, and a later fragment declaring a larger count and index wrote
   past it. Sabrosa's fork checks each fragment's index against its own count, not against the array, so
   this is still open there. Fix: `Add` rejects (returns false, and the caller frees) any fragment
   whose count, index or message id doesn't match the array, before its asserts (which are live in
   debug builds and would read out of bounds).
2. **Unbounded allocation.** `Preallocate` sized an allocation directly from one fragment's
   `splitPacketCount`. **Fixed in Sabrosa's fork:** fragments that would make a message over 4 MB are
   dropped when the packet is parsed (so messages are limited to 4 MB on this branch).
3. **Heap overflow.** `BuildPacketFromSplitPacketList` sized the reassembly buffer by rounding the
   summed bit length to bytes once, then copied each fragment rounded up separately at a bit-derived
   offset. Fix: the buffer is the sum of the per-fragment byte sizes and the copy offset advances in
   bytes. Senders split on byte boundaries, so real messages reassemble exactly as before.

`example_tests` checks 1 against `SplitPacketSort` directly, and sends a 1 MB message (about 750
fragments) through 2 and 3, also under AddressSanitizer.

## 0002: TLS on (configuration)

`defineoverrides.h`, SLikeNet's user-editable settings file, sets `OPEN_SSL_CLIENT_SUPPORT 1`, so
`TCPInterface` (`StartSSLClient`), `HTTPConnection`, `HTTPConnection2` and `Rackspace` can use TLS.
Define `OPEN_SSL_CLIENT_SUPPORT 0` before including SLikeNet to build without it.

## 0003: socket address lengths on macOS (every send failed)

Sabrosa keeps addresses in a union big enough for IPv6, and passed `sizeof` the whole union (28 bytes)
to `sendto` and `getnameinfo` for IPv4 addresses too. Linux accepts a longer length; macOS (and the
other BSDs) reject it with `EINVAL`, so every send failed, starting with RakNet's startup self-test
(`Startup` returned `SOCKET_FAILED_TEST_SEND`). Fix: pass `sizeof(sockaddr_in)` or
`sizeof(sockaddr_in6)` by address family, in `RNS2_Windows_Linux_360` (all game traffic), `Router2` and
`SystemAddress::ToString`.

The startup self-test and the shutdown wake-up also sent to the bound address, which is the wildcard
(`0.0.0.0` / `::`) when listening on all interfaces, and macOS can't send to that either. They now send
to loopback on the bound port when bound to the wildcard, as SLikeNet did before.

## Not needed on this branch

`master`'s other two patches are fixed in Sabrosa's fork: the `UNSET_TIME_US` cast (now an integer, so
`(CCTimeType)-1` is well defined) and the crypto port to OpenSSL 1.1+.
