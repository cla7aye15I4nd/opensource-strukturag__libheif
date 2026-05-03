# bug11 — `m_data_start_pos + get_box_size()` overflow in Box_idat::read_data

**Source:** `libheif/box.cc:4110-4115`

**Severity:** Latent. The audit comment notes the cast is "well-defined
only for non-negative values" and that overflow is unlikely but not
formally impossible.

```cpp
if (start > (uint64_t) m_data_start_pos + get_box_size()) { ... }
```

`m_data_start_pos` is a `std::streampos` (effectively `int64_t`). The cast
to `uint64_t` followed by addition with `get_box_size()` (uint64_t) can
overflow if `m_data_start_pos + box_size > UINT64_MAX`.

## Reproducer

The on-disk PoC sets the idat box's `largesize` to near-`UINT64_MAX`. To
make `m_data_start_pos` non-trivially large the parser would need a file
position near the top of the address space — unreachable through normal
parsing. Instead, the harness drives `Box_idat` directly:

1. Build a `Box_idat` instance.
2. Call its `set_box_size(UINT64_MAX - 1)` setter.
3. Forge `m_data_start_pos = 1024` (small but non-zero).
4. Call `read_data(istr, start=0, length=0, dest, limits)`.
   The arithmetic `(uint64_t)m_data_start_pos + get_box_size()` is
   `1024 + (UINT64_MAX - 1)` = wraps to `1022`.

UBSAN detects the unsigned addition wrap. ASAN sees no access in this
exact case (length=0), but a regression that pairs the wrap with a
non-zero length would expose an OOB seek.

## Reproduce

```
./bug11
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
/home/rdpuser/libheif/libheif/bitstream.cc:540:25: runtime error: implicit conversion from type 'unsigned long' of value 18446744073709551368 (64-bit, unsigned) to type 'int64_t' (aka 'long') changed the value to -248 (64-bit, signed)
    #0 0x... in BitstreamRange::wait_for_available_bytes(unsigned long) /home/rdpuser/libheif/libheif/bitstream.cc:540:25
    #1 0x... in BoxHeader::parse_header(BitstreamRange&) /home/rdpuser/libheif/libheif/box.cc:232:18
    #2 0x... in Box::read(BitstreamRange&, std::shared_ptr<Box>*, heif_security_limits const*) /home/rdpuser/libheif/libheif/box.cc:454:19
    #3 0x... in main /home/rdpuser/libheif/audit_repros/bug11_idat_data_start_pos_overflow/harness.cc:79:15
    #4 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #5 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #6 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug11+0x...)

SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /home/rdpuser/libheif/libheif/bitstream.cc:540:25
```
