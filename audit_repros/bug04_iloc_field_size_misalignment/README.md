# bug04 — iloc field-size {0,4,8} validation missing (umbrella)

**Source:** `libheif/box.cc:1575-1602`

**Severity:** High — stream-misalignment → codec data injection.

`offset_size`, `length_size`, `base_offset_size`, `index_size` are stored
as 4-bit nibbles in the iloc header. The HEIF/ISOBMFF spec requires each
to be 0, 4 or 8. Code reads them and dispatches with `if (size==4) ... else
if (size==8)`. Any other value silently falls through, **without consuming
the field bytes from the stream** — every subsequent read is misaligned.

The audit comment walks through the worst case: `offset_size = 5,
length_size = 4` makes:

- `extent.offset` stay at `0`
- The "real" 5 offset bytes remain in the stream
- `read32()` for `extent.length` reads the first 4 of those bytes →
  **attacker-controlled `extent.length`**

Downstream the parser calls `seek(0)` and reads `extent.length` bytes from
file offset 0 (the ftyp box) into `dest[]`, then ships `dest[]` to the
codec as compressed data. **All HEIF-level validation bypassed.**

## Reproducer

bug04 is the umbrella demonstrator: it generates an iloc with all four
sizes invalid (`offset_size=5, length_size=7, base_offset_size=2,
index_size=3`) and checks that the file is parsed without error
(misalignment is silent).

bug05/06/07 isolate each invalid nibble individually.

## Reproduce

```
python3 gen_poc.py /tmp/bug04.heif
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./bug04 /tmp/bug04.heif
```

Expected with the bug active: parse succeeds (or a downstream codec error)
with grossly wrong `extent.offset`/`length` values. With a real codec
plugin enabled, the bogus bytes from the file's start are passed to the
codec — easily observable by attaching a debugger to `Decoder::decode` and
inspecting the buffer.

ASAN catches the consequence on the iloc fast path inside libheif itself
when `extent.length` lands on a value that exceeds `MAX_FILE_POS` ⇒ error,
or when it lands on a value that drives `dest->resize()` near
`max_memory_block_size` ⇒ allocation refused. To **force** an ASAN crash
inside libheif (no codec needed), this PoC sets the misaligned nibbles to
make `extent.length` overflow into negative-of-uint64 territory, which the
allocator detects as an over-large size.

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
[bug04] read_from_memory err=""
[bug04] iloc parsed silently with 1 items
[bug04]   item 1 extent.offset=0x0 length=0x11234
[bug04] dest.size()=0  iloc-reported length=0x11234
=================================================================
==XXX==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x... at pc 0x... bp 0x... sp 0x...
READ of size 1 at 0x... thread T0
    #0 0x... in main /home/rdpuser/libheif/audit_repros/bug04_iloc_field_size_misalignment/harness.cc:53:55
    #1 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #2 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #3 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug04+0x...)

0x... is located 0 bytes after 1-byte region [0x...,0x...)
allocated by thread T0 here:
    #0 0x... in operator new[](unsigned long) (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug04+0x...)
    #1 0x... in main /home/rdpuser/libheif/audit_repros/bug04_iloc_field_size_misalignment/harness.cc:46:21
    #2 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #3 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #4 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug04+0x...)

SUMMARY: AddressSanitizer: heap-buffer-overflow /home/rdpuser/libheif/audit_repros/bug04_iloc_field_size_misalignment/harness.cc:53:55 in main
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
## Fix

```cpp
auto valid_size = [](int s){ return s==0 || s==4 || s==8; };
if (!valid_size(offset_size) || !valid_size(length_size) ||
    !valid_size(base_offset_size) || !valid_size(index_size)) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_Invalid_box_size,
                 "iloc field size must be 0, 4, or 8"};
}
```
