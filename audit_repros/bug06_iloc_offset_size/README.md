# bug06 — iloc `offset_size` not validated against {0,4,8}

**Source:** `libheif/box.cc:1700-1701`

**Severity:** High — direct codec-data-injection primitive (the audit
comment names this case explicitly).

With `offset_size = 5`:
- `extent.offset` stays `0`.
- 5 offset bytes remain in the stream.
- Next `read32()` for `extent.length` reads bytes 0..3 of those 5 →
  attacker-chosen `extent.length`.
- Box_iloc::read_data uses `extent.offset = 0` → seeks to byte 0 of the
  HEIF file → reads `extent.length` bytes from the ftyp area.
- That blob is shipped to the codec.

## Reproduce

```
python3 gen_poc.py /tmp/bug06.heif
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./bug06 /tmp/bug06.heif
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
read_from_memory: code=0 msg=
[bug06] iloc parsed silently with offset_size=5 (invalid, spec requires 0/4/8)
[bug06] item 1 extent.offset=0x0  extent.length=0x...
[bug06] dest.size()=0  iloc-reported length=0x... (= 305419896)
=================================================================
==XXX==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x... at pc 0x... bp 0x... sp 0x...
READ of size 1 at 0x... thread T0
    #0 0x... in main /home/rdpuser/libheif/audit_repros/bug06_iloc_offset_size/harness.cc:56:55
    #1 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #2 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #3 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug06+0x...)

0x... is located 0 bytes after 1-byte region [0x...,0x...)
allocated by thread T0 here:
    #0 0x... in operator new[](unsigned long) (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug06+0x...)
    #1 0x... in main /home/rdpuser/libheif/audit_repros/bug06_iloc_offset_size/harness.cc:48:21
    #2 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #3 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #4 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug06+0x...)

SUMMARY: AddressSanitizer: heap-buffer-overflow /home/rdpuser/libheif/audit_repros/bug06_iloc_offset_size/harness.cc:56:55 in main
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
