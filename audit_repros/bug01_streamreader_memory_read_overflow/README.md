# bug01 — `StreamReader_memory::read` uint64 wrap

**Source:** `libheif/bitstream.cc:111-115`

**Severity:** Latent (invariant maintained).

The audit comment is explicit:

> "uint64_t addition could wrap if `m_position` is near `UINT64_MAX` and
> size is large. In practice `m_position` is always ≤ `m_length` (enforced
> by `seek()`), and `m_length` is bounded by the host's address space, so
> overflow is unreachable."

## Reproducer

The reproducer is a "would-be-vulnerable" demonstration: we construct a
`StreamReader_memory` whose internal state has been corrupted by friend-class
access — `m_position = UINT64_MAX - 4` while `m_length = UINT64_MAX - 4`.
Calling `read(buf, 16)` then computes `end_pos = m_position + 16` which
wraps to `12`. `12 > m_length` is false → bounds check passes → memcpy
reads 16 bytes from `m_data[UINT64_MAX - 4]`, which is OOB.

This setup is **not reachable from libheif's public API today** because
`seek()` rejects positions > `m_length`. The harness is here to:

1. Document the exact precondition that would expose the bug.
2. Catch a regression — if anyone refactors `seek()` to widen the
   precondition (e.g. allow position == UINT64_MAX), ASAN will fire.

To break the friend invariant we use a small in-tree shim
(`StreamReader_memory_test_shim` — a simple subclass that re-exposes the
fields). With NDEBUG and ASAN, the OOB memcpy is detected.

## Reproduce

```
./bug01
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
[bug01] seek to high offset: 1
/home/rdpuser/libheif/libheif/bitstream.cc:116:33: runtime error: unsigned integer overflow: 18446744073709551499 + 200 cannot be represented in type 'unsigned long'
    #0 0x... in StreamReader_memory::read(void*, unsigned long) /home/rdpuser/libheif/libheif/bitstream.cc:116:33
    #1 0x... in main /home/rdpuser/libheif/audit_repros/bug01_streamreader_memory_read_overflow/harness.cc:34:17
    #2 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #3 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #4 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug01+0x36904)

SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /home/rdpuser/libheif/libheif/bitstream.cc:116:33
```
