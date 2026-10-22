# SLikeNet in ofxRakNet

Source: [SLikeSoft/SLikeNet](https://github.com/SLikeSoft/SLikeNet) `master` at `d5f775d` (2021-07-01),
the last commit. SLikeNet is the maintained fork of RakNet 4.082 (Oculus / Facebook, archived).

Included: the core library, `Source/include/slikenet` and `Source/src`, with the `crypto` helpers and
TLS client support built against openFrameworks' OpenSSL (apothecary's on macOS, iOS, Windows, Android
and Emscripten, the system's through pkg-config on Linux). Left out: samples, dependent extensions, and
the RakNet compatibility headers in `Source/*.h`.

Licenses: `license/license.txt` (SLikeNet, MIT), `license/licenses/RakNet License.txt` (RakNet, BSD)
and `RakNet Patents.txt`, as listed in SLikeNet's `_quick_licensing_slikenet_core.txt`.

The files keep their CRLF line endings, so the patches below apply to upstream as they are:

```sh
cd SLikeNet && git apply ofxRakNet/libs/slikenet/patches/*.patch
```

## 0001: split-packet reassembly bounds (SLikeNet issue #82)

[SLikeNet issue #82](https://github.com/SLikeSoft/SLikeNet/issues/82) (2026-08-24, unanswered) reports
three remote memory-safety defects in reassembling messages split over several datagrams
(`Source/src/ReliabilityLayer.cpp`, the default `SplitPacketSort` path). Any peer, connected or not, can
send the fragments.

1. **Out-of-bounds write.** `SplitPacketSort::Add` indexed its array with the fragment's
   `splitPacketIndex`, checked only by `RakAssert`, which is empty in release builds. The array is sized
   from the first fragment of a message, and a later fragment declaring a larger count and index wrote
   past it. Fix: `Add` rejects (returns false, and the caller frees) any fragment whose count, index or
   message id doesn't match the array.
2. **Unbounded allocation.** `Preallocate` sized an allocation directly from one fragment's
   `splitPacketCount` (up to ~17 GB, and a negative size through an `(int)` cast). Fix: fragments with
   a count of 0 or above `SLIKENET_MAX_SPLIT_PACKET_COUNT` (65536 by default, ~90 MB messages at a
   1400 byte MTU; define it to change) are dropped when the packet is parsed.
3. **Heap overflow.** `BuildPacketFromSplitPacketList` sized the reassembly buffer by rounding the
   summed bit length to bytes once, then copied each fragment rounded up separately at a bit-derived
   offset. Fix: the buffer is the sum of the per-fragment byte sizes and the copy offset advances in
   bytes. Senders split on byte boundaries, so real messages reassemble exactly as before.

`example_tests` checks 1 against `SplitPacketSort` directly, and sends a 1 MB message (about 750
fragments) through 2 and 3, also under AddressSanitizer.

## 0002: undefined behaviour in the congestion control (crash on the first ACK)

`CCRakNetSlidingWindow.cpp` (the default congestion control) and `CCRakNetUDT.cpp` cast
`UNSET_TIME_US`, a `double` equal to -1, to the unsigned `CCTimeType`. A negative `double` converted to
an unsigned integer is undefined behaviour, and current clang (Xcode 26) compiles
`CCRakNetSlidingWindow::ShouldSendACKs` to a single `brk` instruction: every process crashed
(`EXC_BREAKPOINT` on SLikeNet's network thread) as soon as a connection had an ACK to send. Fix: compare
and return `UNSET_TIME_CC`, `(CCTimeType)-1`, which is well defined and the same value SLikeNet meant.

## 0003: crypto helpers on OpenSSL 1.1+

SLikeNet's `crypto` code (`CCryptoManager` session encryption, `CFileEncrypter` signing) was written
for OpenSSL 1.0 and doesn't build with 1.1, 3.x or the 4.x openFrameworks ships:

- `EVP_CIPHER_CTX` is opaque since 1.1: the two static contexts are now pointers, allocated with
  `EVP_CIPHER_CTX_new()` in `Initialize()`.
- `EVP_MD_CTX_cleanup` was removed: `EVP_MD_CTX_reset`.
- `RAND_pseudo_bytes` (deprecated, not cryptographically strong) for the IV: `RAND_bytes`.

All behind `OPENSSL_VERSION_NUMBER >= 0x10100000L`, so OpenSSL 1.0.2 still builds the original code.
`example_tests` encrypts and decrypts through `CCryptoManager` (AES-256-CBC).

## 0004: TLS on (configuration)

`defineoverrides.h`, SLikeNet's user-editable settings file, sets `OPEN_SSL_CLIENT_SUPPORT 1`, so
`TCPInterface` (`StartSSLClient`), `HTTPConnection`, `HTTPConnection2` and `Rackspace` can use TLS.
Define `OPEN_SSL_CLIENT_SUPPORT 0` before including SLikeNet to build without it.
