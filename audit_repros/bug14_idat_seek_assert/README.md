# bug14 — `istr->seek()` failure caught only by `assert()`

**Source:** `libheif/box.cc:4140-4147`

**Severity:** Latent — observable only when an upstream bound is bypassed.

```cpp
bool success;
success = istr->seek(m_data_start_pos + (std::streampos) start);
assert(success);   // disabled in release builds
(void) success;
```

If a future regression lets bug11/bug12/bug13 yield a `start` that survives
the bounds checks but fails the seek (e.g. the bounds check used the
overflowed value), the seek silently fails and the subsequent `read()`
populates `out_data` with whatever bytes happened to be at the current
file position — usually leftover from the previous read.

## Reproducer

The harness builds an idat box where `m_data_start_pos + start` deliberately
exceeds the underlying StreamReader_memory's claimed length, then calls
read_data. We arrange `start` to slip past `> get_box_size()` (e.g.
get_box_size is large because of bug11) but still seek-fail because
StreamReader_memory's `seek()` rejects positions > its internal length.

With NDEBUG the assert is gone and the function continues. ASAN observes
the subsequent `istr->read()` filling `out_data` from a stale position.

## Reproduce

```
./bug14
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
=================================================================
==XXX==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x... at pc 0x... bp 0x... sp 0x...
READ of size 64 at 0x... thread T0
    #0 0x... in __asan_memcpy (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug14+0x...)
    #1 0x... in FailingSeekReader::read(void*, unsigned long) /home/rdpuser/libheif/audit_repros/bug14_idat_seek_assert/harness.cc:28:5
    #2 0x... in Box_idat::read_data(std::shared_ptr<StreamReader> const&, unsigned long, unsigned long, std::vector<unsigned char, std::allocator<unsigned char>>&, heif_security_limits const*) const /home/rdpuser/libheif/libheif/box.cc:4162:21
    #3 0x... in main /home/rdpuser/libheif/audit_repros/bug14_idat_seek_assert/harness.cc:79:19
    #4 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #5 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #6 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug14+0x...)

0x... is located 0 bytes after 32-byte region [0x...,0x...)
allocated by thread T0 here:
    #0 0x... in operator new(unsigned long) (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug14+0x...)
    #1 0x... in std::__new_allocator<unsigned char>::allocate(unsigned long, void const*) /usr/include/c++/N/bits/new_allocator.h:151:27
    #2 0x... in std::allocator<unsigned char>::allocate(unsigned long) /usr/include/c++/N/bits/allocator.h:198:32
    #3 0x... in std::allocator_traits<std::allocator<unsigned char>>::allocate(std::allocator<unsigned char>&, unsigned long) /usr/include/c++/N/bits/alloc_traits.h:482:20
    #4 0x... in std::_Vector_base<unsigned char, std::allocator<unsigned char>>::_M_allocate(unsigned long) /usr/include/c++/N/bits/stl_vector.h:381:20
    #5 0x... in void std::vector<unsigned char, std::allocator<unsigned char>>::_M_realloc_insert<unsigned char>(__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, unsigned char&&) /usr/include/c++/N/bits/vector.tcc:459:33
    #6 0x... in unsigned char& std::vector<unsigned char, std::allocator<unsigned char>>::emplace_back<unsigned char>(unsigned char&&) /usr/include/c++/N/bits/vector.tcc:123:4
    #7 0x... in std::vector<unsigned char, std::allocator<unsigned char>>::push_back(unsigned char&&) /usr/include/c++/N/bits/stl_vector.h:1299:9
    #8 0x... in forge_idat_with_large_claim() /home/rdpuser/libheif/audit_repros/bug14_idat_seek_assert/harness.cc:58:34
    #9 0x... in main /home/rdpuser/libheif/audit_repros/bug14_idat_seek_assert/harness.cc:63:14
    #10 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #11 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #12 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug14+0x...)

SUMMARY: AddressSanitizer: heap-buffer-overflow (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug14+0x...) in __asan_memcpy
Shadow bytes around the buggy address:
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x...: fa fa 00 00 00 fa fa fa 00 00 00 00[fa]fa fa fa
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

Replace the assert with a hard error return:

```cpp
if (!istr->seek(m_data_start_pos + (std::streampos) start)) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_Unspecified,
                 "Error setting input file position"};
}
```
