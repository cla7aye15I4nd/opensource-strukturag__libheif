# bug07 — iloc `length_size` not validated against {0,4,8}

**Source:** `libheif/box.cc:1711-1715`

**Severity:** High — same primitive as bug06, applied to `length_size`.

With `length_size = 7`:
- `extent.length` stays `0`.
- 7 length bytes remain in the stream.
- For multi-extent or multi-item iloc, those bytes corrupt the next field.

## Reproduce

```
python3 gen_poc.py /tmp/bug07.heif
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./bug07 /tmp/bug07.heif
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
[bug07] read_from_memory err=""
[bug07] iloc parsed silently with length_size=7 (invalid)
[bug07] item 1 extent.offset=0x0 length=0x0
[bug07] dest.size()=0  on-disk-encoded length=0x...
=================================================================
==XXX==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x... at pc 0x... bp 0x... sp 0x...
READ of size 1 at 0x... thread T0
    #0 0x... in main /home/rdpuser/libheif/audit_repros/bug07_iloc_length_size/harness.cc:60:57
    #1 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #2 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #3 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug07+0x...)

0x... is located 0 bytes after 1-byte region [0x...,0x...)
allocated by thread T0 here:
    #0 0x... in operator new[](unsigned long) (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug07+0x...)
    #1 0x... in main /home/rdpuser/libheif/audit_repros/bug07_iloc_length_size/harness.cc:45:21
    #2 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #3 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #4 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug07+0x...)

SUMMARY: AddressSanitizer: heap-buffer-overflow /home/rdpuser/libheif/audit_repros/bug07_iloc_length_size/harness.cc:60:57 in main
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
