# bug13 — signed/unsigned mix in Box_idat::read_data wait_for_file_size

**Source:** `libheif/box.cc:4127-4131`

**Severity:** Low — wrong error, not memory unsafety.

```cpp
StreamReader::grow_status status =
    istr->wait_for_file_size((int64_t) m_data_start_pos + start + length);
```

`m_data_start_pos` cast to `int64_t`, then added to two `uint64_t`s
(`start`, `length`). Operands are widened to the largest unsigned type
involved, but the explicit `(int64_t)` cast first forces `m_data_start_pos`
into a signed type — for very large values the cast itself is UB
(implementation-defined narrowing).

UBSAN catches the signed-overflow when `m_data_start_pos + start + length`
exceeds `INT64_MAX`.

## Reproducer

The harness builds a forged idat with a large `m_data_start_pos` (via
seeking the underlying stream first) and calls read_data with
`start = INT64_MAX/2, length = INT64_MAX/2` to push the addition past
`INT64_MAX`. UBSAN flags `signed-integer-overflow`.

## Fix

Use `uint64_t` for the temp:

```cpp
uint64_t target = (uint64_t)m_data_start_pos + start + length;
```

(plus a saturation guard).

## Reproduce

```
UBSAN_OPTIONS=halt_on_error=1 ./bug13
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
/home/rdpuser/libheif/libheif/bitstream.cc:540:25: runtime error: implicit conversion from type 'unsigned long' of value 18446744073709551368 (64-bit, unsigned) to type 'int64_t' (aka 'long') changed the value to -248 (64-bit, signed)
    #0 0x... in BitstreamRange::wait_for_available_bytes(unsigned long) /home/rdpuser/libheif/libheif/bitstream.cc:540:25
    #1 0x... in BoxHeader::parse_header(BitstreamRange&) /home/rdpuser/libheif/libheif/box.cc:232:18
    #2 0x... in Box::read(BitstreamRange&, std::shared_ptr<Box>*, heif_security_limits const*) /home/rdpuser/libheif/libheif/box.cc:454:19
    #3 0x... in main /home/rdpuser/libheif/audit_repros/bug13_idat_signed_unsigned_mix/harness.cc:62:3
    #4 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #5 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #6 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug13+0x...)

SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /home/rdpuser/libheif/libheif/bitstream.cc:540:25
```
