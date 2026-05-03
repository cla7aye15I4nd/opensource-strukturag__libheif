# bug15 — `DataExtent::read_data` OOB iterator advance

**Source:** `libheif/codecs/decoder.cc:96-106`

**Severity:** Latent. The audit comment is explicit:

> "In the current code flow DataExtent is always freshly constructed before
> decode, so m_raw starts empty and the Source::Image branch below (which is
> bounds-checked via iloc) is taken instead. However, if future code reuses
> a DataExtent across calls, or if an adversary triggers the iloc field-size
> misalignment bug (BUG-2) so that the first read_data() populates m_raw
> with fewer bytes than expected, a subsequent read_data(offset, size) call
> with offset+size > m_raw.size() would read out of bounds."

```cpp
if (!m_raw.empty()) {
  data.insert(data.begin(),
              m_raw.begin() + offset,
              m_raw.begin() + offset + size);
  return data;
}
```

No bounds check on `offset + size > m_raw.size()`. The iterator arithmetic
advances past `m_raw.end()`, which is UB and an OOB read.

## Reproducer

`m_raw` is a public `mutable std::vector<uint8_t>`. The harness:

1. Constructs a `DataExtent`.
2. Pushes 16 bytes into `m_raw` (simulating the cached / partial-fill path).
3. Calls `read_data(/*offset=*/0, /*size=*/64)`.

ASAN flags the heap-buffer-overflow on the iterator advance.

## Reproduce

```
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./bug15
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
=================================================================
==XXX==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x... at pc 0x... bp 0x... sp 0x...
READ of size 64 at 0x... thread T0
    #0 0x... in __asan_memmove (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug15+0x...)
    #1 0x... in unsigned char* std::__copy_move<false, true, std::random_access_iterator_tag>::__copy_m<unsigned char, unsigned char>(unsigned char*, unsigned char*, unsigned char*) /usr/include/c++/N/bits/stl_algobase.h:437:6
    #2 0x... in unsigned char* std::__copy_move_a2<false, unsigned char*, unsigned char*>(unsigned char*, unsigned char*, unsigned char*) /usr/include/c++/N/bits/stl_algobase.h:505:14
    #3 0x... in unsigned char* std::__copy_move_a1<false, unsigned char*, unsigned char*>(unsigned char*, unsigned char*, unsigned char*) /usr/include/c++/N/bits/stl_algobase.h:533:14
    #4 0x... in unsigned char* std::__copy_move_a<false, __gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, unsigned char*>(__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, __gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, unsigned char*) /usr/include/c++/N/bits/stl_algobase.h:541:3
    #5 0x... in unsigned char* std::copy<__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, unsigned char*>(__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, __gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, unsigned char*) /usr/include/c++/N/bits/stl_algobase.h:632:14
    #6 0x... in unsigned char* std::__uninitialized_copy<true>::__uninit_copy<__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, unsigned char*>(__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, __gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, unsigned char*) /usr/include/c++/N/bits/stl_uninitialized.h:147:18
    #7 0x... in unsigned char* std::uninitialized_copy<__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, unsigned char*>(__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, __gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, unsigned char*) /usr/include/c++/N/bits/stl_uninitialized.h:184:14
    #8 0x... in unsigned char* std::__uninitialized_copy_a<__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, unsigned char*, unsigned char>(__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, __gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, unsigned char*, std::allocator<unsigned char>&) /usr/include/c++/N/bits/stl_uninitialized.h:373:14
    #9 0x... in void std::vector<unsigned char, std::allocator<unsigned char>>::_M_range_insert<__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>>(__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, __gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, __gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, std::forward_iterator_tag) /usr/include/c++/N/bits/vector.tcc:814:11
    #10 0x... in __gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>> std::vector<unsigned char, std::allocator<unsigned char>>::insert<__gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, void>(__gnu_cxx::__normal_iterator<unsigned char const*, std::vector<unsigned char, std::allocator<unsigned char>>>, __gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>, __gnu_cxx::__normal_iterator<unsigned char*, std::vector<unsigned char, std::allocator<unsigned char>>>) /usr/include/c++/N/bits/stl_vector.h:1486:4
    #11 0x... in DataExtent::read_data(unsigned long, unsigned long) const /home/rdpuser/libheif/libheif/codecs/decoder.cc:106:10
    #12 0x... in main /home/rdpuser/libheif/audit_repros/bug15_dataextent_oob_iter/harness.cc:16:18
    #13 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #14 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #15 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug15+0x...)

0x... is located 0 bytes after 16-byte region [0x...,0x...)
allocated by thread T0 here:
    #0 0x... in operator new(unsigned long) (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug15+0x...)
    #1 0x... in std::__new_allocator<unsigned char>::allocate(unsigned long, void const*) /usr/include/c++/N/bits/new_allocator.h:151:27
    #2 0x... in std::allocator<unsigned char>::allocate(unsigned long) /usr/include/c++/N/bits/allocator.h:198:32
    #3 0x... in std::allocator_traits<std::allocator<unsigned char>>::allocate(std::allocator<unsigned char>&, unsigned long) /usr/include/c++/N/bits/alloc_traits.h:482:20
    #4 0x... in std::_Vector_base<unsigned char, std::allocator<unsigned char>>::_M_allocate(unsigned long) /usr/include/c++/N/bits/stl_vector.h:381:20
    #5 0x... in std::_Vector_base<unsigned char, std::allocator<unsigned char>>::_M_create_storage(unsigned long) /usr/include/c++/N/bits/stl_vector.h:398:33
    #6 0x... in std::_Vector_base<unsigned char, std::allocator<unsigned char>>::_Vector_base(unsigned long, std::allocator<unsigned char> const&) /usr/include/c++/N/bits/stl_vector.h:335:9
    #7 0x... in std::vector<unsigned char, std::allocator<unsigned char>>::vector(unsigned long, unsigned char const&, std::allocator<unsigned char> const&) /usr/include/c++/N/bits/stl_vector.h:571:9
    #8 0x... in std::vector<unsigned char, std::allocator<unsigned char>>::_M_fill_assign(unsigned long, unsigned char const&) /usr/include/c++/N/bits/vector.tcc:275:11
    #9 0x... in std::vector<unsigned char, std::allocator<unsigned char>>::assign(unsigned long, unsigned char const&) /usr/include/c++/N/bits/stl_vector.h:809:9
    #10 0x... in main /home/rdpuser/libheif/audit_repros/bug15_dataextent_oob_iter/harness.cc:12:13
    #11 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #12 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #13 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug15+0x...)

SUMMARY: AddressSanitizer: heap-buffer-overflow (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug15+0x...) in __asan_memmove
Shadow bytes around the buggy address:
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x...: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x...: fa fa 00 fa fa fa 00 00[fa]fa fa fa fa fa fa fa
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
... (truncated 10 more lines: shadow-bytes dump and legend) ...
```
## Fix

Add a bounds check at the top of the if-branch:

```cpp
if (offset > m_raw.size() || size > m_raw.size() - offset) {
    return Error{heif_error_Invalid_input, heif_suberror_End_of_data,
                 "DataExtent::read_data offset+size exceeds cached buffer"};
}
```
