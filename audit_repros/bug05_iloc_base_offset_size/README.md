# bug05 — iloc `base_offset_size` not validated against {0,4,8}

**Source:** `libheif/box.cc:1653-1655`

**Severity:** High (specific case of bug04).

```cpp
item.base_offset = 0;
if (base_offset_size == 4) { ... }
else if (base_offset_size == 8) { ... }
// no else — and no error if base_offset_size is e.g. 2
```

With `base_offset_size = 2`, neither branch is taken, the 2-byte
`base_offset` field stays unread in the stream, and the next `read16()`
for `extent_count` consumes those 2 bytes — yielding an
attacker-controlled extent count.

## Reproduce

```
python3 gen_poc.py /tmp/bug05.heif
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./bug05 /tmp/bug05.heif
```

The PoC sets `base_offset_size=2` and packs the would-be-consumed bytes
to make `extent_count` very large (capped by
`limits->max_iloc_extents_per_item`, default 32). The `extent.length`
then ends up reading data from where the real `extent_count` and other
real fields lived, producing a wrong-but-not-rejected parse.

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
[bug05] read_from_memory err=""
[bug05] iloc parsed silently with base_offset_size=2 (invalid)
[bug05] item 1 extent.offset=0x12222  extent.length=0x...
[bug05] dest.size()=0  iloc-reported length=0x...
=================================================================
==XXX==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x... at pc 0x... bp 0x... sp 0x...
READ of size 1 at 0x... thread T0
    #0 0x... in main /home/rdpuser/libheif/audit_repros/bug05_iloc_base_offset_size/harness.cc:53:55
    #1 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #2 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #3 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug05+0x...)

0x... is located 0 bytes after 1-byte region [0x...,0x...)
allocated by thread T0 here:
    #0 0x... in operator new[](unsigned long) (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug05+0x...)
    #1 0x... in main /home/rdpuser/libheif/audit_repros/bug05_iloc_base_offset_size/harness.cc:46:21
    #2 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #3 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #4 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug05+0x...)

SUMMARY: AddressSanitizer: heap-buffer-overflow /home/rdpuser/libheif/audit_repros/bug05_iloc_base_offset_size/harness.cc:53:55 in main
Shadow bytes around the buggy address:
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: fa fa 00 fa fa fa fd fd fa fa fd fd fa fa fd fa
  0x...: fa fa 00 fa fa fa fd fd fa fa fd fd fa fa 00 00
=>0x...: fa fa fd fd fa fa fd fd fa fa fd fd fa fa[01]fa
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
