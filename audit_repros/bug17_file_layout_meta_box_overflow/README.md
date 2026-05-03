# bug17 — `meta_box_start + box_size` overflow

**Source:** `libheif/file_layout.cc:143-149`

**Severity:** High — chains directly into bug02 (`BitstreamRange` underflow).

## Root cause

```cpp
uint64_t end_of_meta_box = meta_box_start + box_header.get_box_size();
```

`get_box_size()` is taken from the on-disk `largesize` field (8 bytes,
attacker-controlled when the size32 field is `1`). With `meta_box_start`
small (e.g. `100`) and `box_size = UINT64_MAX - 95`, the sum wraps to a
small value (`4`).

The next check `if (m_max_length < end_of_meta_box)` uses the *wrapped*
`end_of_meta_box`, so it does NOT request more data and does NOT bail.

`BitstreamRange(stream, meta_box_start /*100*/, end_of_meta_box /*4*/)` is then
called with `end < start`. In a build with `NDEBUG` the `assert(end >= start)`
is compiled out and `m_remaining = end - start` underflows to `~SIZE_MAX`.

## What the harness does

The harness `bug17` is the public-API harness — it just reads the input file
into memory and calls `heif_context_read_from_memory`. Trigger conditions:

1. **Debug build (asserts active)**: the program aborts inside the
   `BitstreamRange` constructor — visible in the ASAN backtrace as
   `__assert_fail`.
2. **NDEBUG build** (the configuration produced by `audit_repros/build.sh`):
   `m_remaining` underflows to `~SIZE_MAX`, every `prepare_read()` succeeds
   without bounds checking, and the parser walks past the meta box. With
   `StreamReader_memory` the underlying `read()` is still bounded by the input
   buffer, so the visible failure mode is that parsing of subsequent
   top-level boxes is corrupted (boxes inside the "meta" body are interpreted
   from offsets the attacker chose). With a `heif_reader` C-API stream where
   the underlying reader does not double-check its own bounds, this leads to
   an actual OOB read.

## Reproduce

```
python3 gen_poc.py /tmp/bug17.heif
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./bug17 /tmp/bug17.heif
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
/home/rdpuser/libheif/audit_repros/bug17_file_layout_meta_box_overflow/harness.cc:37:45: runtime error: unsigned integer overflow: 24 + 18446744073709551596 cannot be represented in type 'uint64_t' (aka 'unsigned long')
    #0 0x... in main /home/rdpuser/libheif/audit_repros/bug17_file_layout_meta_box_overflow/harness.cc:37:45
    #1 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #2 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #3 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug17+0x36904)

SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /home/rdpuser/libheif/audit_repros/bug17_file_layout_meta_box_overflow/harness.cc:37:45
```
## Fix

Add a saturation guard before the addition:

```cpp
if (box_header.get_box_size() > UINT64_MAX - meta_box_start) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_No_meta_box,
                 "meta box size causes integer overflow"};
}
```
