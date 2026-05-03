# bug10 — `size -= extent.length` underflow

**Source:** `libheif/box.cc:1962-1968`

**Severity:** High.

```cpp
idat->read_data(istr,
                extent.offset + item->base_offset,
                extent.length,
                *dest, limits);
size -= extent.length;
```

`size` is the caller's requested limit (e.g. "give me at most N bytes").
If `extent.length > size`, the subtraction wraps to `~0ULL`. On the next
loop iteration the construction_method==0 branch computes
`read_len = std::min(extent.length - skip_len, size)` — with `size = ~0ULL`,
the min returns `extent.length`, which can far exceed the caller's
intended cap. The `dest` buffer is then resized to a larger value than
the caller budgeted for (the per-block allocation cap still applies, but
the caller's logical contract is violated).

The audit comment also notes a second consequence: the idat branch ignores
the `size` parameter entirely (always reads `extent.length`). Combined
with the underflow, this lets a multi-extent iloc copy more bytes into
`dest` than the caller asked for.

## Reproducer

The PoC builds an iloc with **two extents**:
- Extent 1: construction_method=1 (idat), `length = 0x1000` — far larger
  than the caller will request (the metadata fetch path requests exactly
  the iloc-reported length, but the audit specifically calls out the case
  where a future caller passes a smaller `size`).
- Extent 2: construction_method=0 (file), `length = 0x10` — tiny.

The harness drives `Box_iloc::read_data` directly with `offset=0, size=4`
to expose the underflow. Direct invocation is needed because the public
API always passes `UINT64_MAX` for `size`. Once the underflow has occurred,
ASAN flags the over-large `dest->resize` against `max_memory_block_size`,
or — when limits are wide — flags the OOB write that follows.

## Reproduce

```
python3 gen_poc.py /tmp/bug10.heif
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./bug10 /tmp/bug10.heif
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
[bug10] HeifFile::read_from_memory: 
/home/rdpuser/libheif/libheif/box.cc:1969:12: runtime error: unsigned integer overflow: 4 - 4096 cannot be represented in type 'uint64_t' (aka 'unsigned long')
    #0 0x... in Box_iloc::read_data(unsigned int, std::shared_ptr<StreamReader> const&, std::shared_ptr<Box_idat> const&, std::vector<unsigned char, std::allocator<unsigned char>>*, unsigned long, unsigned long, heif_security_limits const*) const /home/rdpuser/libheif/libheif/box.cc:1969:12
    #1 0x... in HeifFile::append_data_from_iloc(unsigned int, std::vector<unsigned char, std::allocator<unsigned char>>&, unsigned long, unsigned long) const /home/rdpuser/libheif/libheif/file.cc:896:22
    #2 0x... in main /home/rdpuser/libheif/audit_repros/bug10_iloc_size_underflow/harness.cc:40:20
    #3 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #4 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #5 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug10+0x...)

SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /home/rdpuser/libheif/libheif/box.cc:1969:12
```
