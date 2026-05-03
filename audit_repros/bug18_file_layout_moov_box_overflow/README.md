# bug18 — `moov_box_start + box_size` overflow

**Source:** `libheif/file_layout.cc:220-223`

**Severity:** High — same primitive as bug17, but in the `moov` branch.

Same root cause and consequence as bug17 — see that README for the chain
into `BitstreamRange` underflow. The only difference is that the wrapping
`largesize` is on a `moov` box instead of a `meta` box, which exercises the
sequences/track-parsing path rather than the still-image meta path.

`moov` is the entry point for HEIF sequences; ftyp must include a brand
that triggers sequence parsing (we use `msf1`/`heic`).

## Reproduce

```
python3 gen_poc.py /tmp/bug18.heif
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./bug18 /tmp/bug18.heif
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
/home/rdpuser/libheif/audit_repros/bug18_file_layout_moov_box_overflow/harness.cc:19:45: runtime error: unsigned integer overflow: 28 + 18446744073709551592 cannot be represented in type 'uint64_t' (aka 'unsigned long')
    #0 0x... in main /home/rdpuser/libheif/audit_repros/bug18_file_layout_moov_box_overflow/harness.cc:19:45
    #1 0x...  (/lib/x86_64-linux-gnu/libc.so.6+0x2a1c9)
    #2 0x... in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x2a28a)
    #3 0x... in _start (/home/rdpuser/libheif/build-audit-asan/audit_repros/bug18+0x36904)

SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /home/rdpuser/libheif/audit_repros/bug18_file_layout_moov_box_overflow/harness.cc:19:45
```
## Fix

Identical to bug17 — saturation check before the addition.
