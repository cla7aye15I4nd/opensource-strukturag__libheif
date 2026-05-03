# bug02 — `BitstreamRange(stream, start, end)` underflow when `end < start`

**Source:** `libheif/bitstream.cc:175-181`

**Severity:** High — the highest-severity downstream consequence of the
`file_layout.cc` overflow bugs (bug17 + bug18). The audit comment names this
out explicitly:

> "This is the highest-severity consequence of the overflow bugs in
> file_layout.cc. The assert must be replaced by a hard runtime check."

## Root cause

```cpp
BitstreamRange::BitstreamRange(istr, size_t start, size_t end)
    : ..., m_remaining(end - start)
{
  assert(end >= start);   // disabled in release builds (NDEBUG)
  ...
}
```

When the caller computes `end` as `start + box_size` and that overflows
(bug17/bug18), `end < start`. The assert protects debug builds but is a
no-op under `-DNDEBUG`. `m_remaining` is `size_t`, so the subtraction wraps
to `~SIZE_MAX`. Every subsequent `prepare_read(N)` returns true because
`m_remaining >= N` is trivially satisfied. Bounds checking is gone for the
rest of the parse.

## Reproducer

The same input as bug17 also exercises bug02 — the constructor is the
*chained* sink, file_layout the *source*. We ship a separate harness here
because the audit lists this as a distinct entry, and because keeping it
separate lets a future fix be unit-tested locally even when the file_layout
overflow is mitigated elsewhere.

`gen_poc.py` produces the same wrapping-meta-box payload as bug17.

## Reproduce

```
python3 gen_poc.py /tmp/bug02.heif
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./bug02 /tmp/bug02.heif
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
/home/rdpuser/libheif/libheif/bitstream.cc:173:46: runtime error: unsigned integer overflow: 4 - 100 cannot be represented in type 'size_t' (aka 'unsigned long')
    #0 0x... in BitstreamRange::BitstreamRange(std::shared_ptr<StreamReader>, unsigned long, unsigned long) /home/rdpuser/libheif/libheif/bitstream.cc:173:46
    #1 0x... in main /home/rdpuser/libheif/audit_repros/bug02_bitstreamrange_end_lt_start/harness.cc:36:18
    #2 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #3 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #4 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug02+0x36904)

SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /home/rdpuser/libheif/libheif/bitstream.cc:173:46
```
## Fix

Replace the assert with a runtime check:

```cpp
if (end < start) {
    m_remaining = 0;
    m_error = true;
    return;
}
m_remaining = end - start;
```
