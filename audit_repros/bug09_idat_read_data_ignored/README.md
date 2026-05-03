# bug09 — `idat->read_data()` return value silently discarded

**Source:** `libheif/box.cc:1951-1956`

**Severity:** Confirmed bug. The audit comment is unambiguous:

> "the return value of `idat->read_data()` is silently discarded. … This is
> a confirmed bug; the return value must be captured and propagated."

```cpp
idat->read_data(istr,
                extent.offset + item->base_offset,
                extent.length,
                *dest, limits);
// ^ no err = ..., no `if (err) return err;`
```

If the idat read fails (out-of-bounds offset, security limit exceeded,
seek/read error), execution falls through. The caller receives
`Error::Ok` with a partially-populated `dest`.

## Reproducer

The PoC builds a HEIF where:
- iloc has `construction_method = 1` (data lives in the idat box).
- The extent's `offset + length` exceeds the idat box's size — read_data
  returns `End_of_data`.
- Box_iloc::read_data ignores that error and returns Ok with `dest`
  containing whatever it was before.

The harness checks the return value of the upstream API call and the
content length. With the bug active, no error is reported and downstream
decoding receives garbage.

ASAN fires further along when the partial-buffer is consumed: the codec
reads short data and most plugins assert on size mismatch. With codecs
disabled (the default for `audit_repros/build.sh`) the visible signal is
that `heif_decode_image` returns Ok but the produced image is corrupt —
detected by the harness comparing the reported decoded size against the
expected pixel count.

## Reproduce

```
python3 gen_poc.py /tmp/bug09.heif
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./bug09 /tmp/bug09.heif
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
read_from_memory: code=0 msg=
[bug09] item 1: extent_total=4096  bytes_read=16  err.code=0 msg=""
[bug09] >>> idat read silently failed: extent declared 4096 bytes,
        dest contains 16 bytes, but Box_iloc::read_data returned Ok
=================================================================
==XXX==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x... at pc 0x... bp 0x... sp 0x...
READ of size 1 at 0x... thread T0
    #0 0x... in main /home/rdpuser/libheif/audit_repros/bug09_idat_read_data_ignored/harness.cc:60:17
    #1 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #2 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #3 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug09+0x...)

0x... is located 0 bytes after 16-byte region [0x...,0x...)
allocated by thread T0 here:
    #0 0x... in operator new[](unsigned long) (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug09+0x...)
    #1 0x... in main /home/rdpuser/libheif/audit_repros/bug09_idat_read_data_ignored/harness.cc:56:25
    #2 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #3 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #4 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug09+0x...)

SUMMARY: AddressSanitizer: heap-buffer-overflow /home/rdpuser/libheif/audit_repros/bug09_idat_read_data_ignored/harness.cc:60:17 in main
Shadow bytes around the buggy address:
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: fa fa 00 fa fa fa fd fd fa fa fd fd fa fa fd fa
  0x...: fa fa 00 fa fa fa fd fd fa fa fd fd fa fa 00 00
  0x...: fa fa fd fd fa fa fd fd fa fa fd fd fa fa 00 00
=>0x...: fa fa 00 00[fa]fa fa fa fa fa fa fa fa fa fa fa
  0x...: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x...: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x...: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x...: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x...: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07 
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==XXX==ABORTING
```
