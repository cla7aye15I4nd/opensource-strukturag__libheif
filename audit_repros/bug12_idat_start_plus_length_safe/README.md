# bug12 — `start + length > get_box_size()` analysis

**Source:** `libheif/box.cc:4116-4117`

**Severity:** Safe (proven). The audit comment is the proof:

> "start and length are each bounded by MAX_FILE_POS (2^55-1), so their sum
> is at most 2*(2^55-1) which cannot overflow uint64_t. Safe."

The reproducer is a regression watchdog. It exercises the bound by passing
`start = MAX_FILE_POS, length = MAX_FILE_POS` to `Box_idat::read_data`.
UBSAN should not flag the addition.

## Reproduce

```
UBSAN_OPTIONS=halt_on_error=1 ./bug12
```

## Real sanitizer output

Captured by running `./build.sh && ./run_all.sh`. Reproduced verbatim below (process IDs, hex addresses and BuildIds normalized for stable diffs):

```
[bug12] read_data err="idat box contained 36028797018963967 bytes, total memory size would be 36028797018963967 bytes, exceeding the security limit of 4294967296 bytes" (UBSAN must not fire)
```
