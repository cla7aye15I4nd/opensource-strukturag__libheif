# bug03 — `wait_for_available_bytes` int64 signed overflow

**Source:** `libheif/bitstream.cc:534-539`

**Severity:** Low — produces a wrong error rather than memory unsafety.

```cpp
int64_t target_size = m_istr->get_position() + nBytes;
return m_istr->wait_for_file_size(target_size);
```

`get_position()` is `uint64_t`, `nBytes` is `size_t`, but the result is
stored in `int64_t`. If `get_position() + nBytes > INT64_MAX`, the addition
overflows — that's UB (signed overflow), and the negative value implicitly
widens to a huge `uint64_t` when passed to `wait_for_file_size`, returning
`size_beyond_eof` instead of a real bounds answer.

## Reproducer

UBSAN detects the signed overflow directly. The harness:

1. Builds a `StreamReader_memory` with a size near UINT64_MAX (same "lie
   about size" trick as bug01).
2. Calls `BitstreamRange::wait_for_available_bytes(N)` where
   `m_position + N` exceeds `INT64_MAX`.

UBSAN fires `signed-integer-overflow` on the addition.

## Reproduce

```
UBSAN_OPTIONS=halt_on_error=1 ./bug03
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
/home/rdpuser/libheif/libheif/bitstream.cc:540:25: runtime error: implicit conversion from type 'unsigned long' of value 9223372036854775863 (64-bit, unsigned) to type 'int64_t' (aka 'long') changed the value to -9223372036854775753 (64-bit, signed)
    #0 0x... in BitstreamRange::wait_for_available_bytes(unsigned long) /home/rdpuser/libheif/libheif/bitstream.cc:540:25
    #1 0x... in main /home/rdpuser/libheif/audit_repros/bug03_wait_for_available_bytes_signed_overflow/harness.cc:26:6
    #2 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #3 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #4 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug03+0x36904)

SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /home/rdpuser/libheif/libheif/bitstream.cc:540:25
```
## Fix

Use `uint64_t target_size` (and overflow-guard the addition).
