# Audit Reproducers

One reproducer per `AUDIT` comment introduced in commit `af303475`
("Enhance safety checks in bitstream and box parsing").

Each `bugNN_*/` directory contains:

| File | Purpose |
| --- | --- |
| `gen_poc.py` | Python generator that emits the malicious / boundary input |
| `harness.cc` | Standalone ASAN/UBSAN harness, built by the top-level CMake |
| `README.md` | Source location, severity, root cause, expected ASAN output |

## Bug index

| # | File:Line | Severity | Title |
|---|-----------|----------|-------|
| 01 | `bitstream.cc:111` | Latent (invariant maintained) | `StreamReader_memory::read` uint64 wrap |
| 02 | `bitstream.cc:175` | **High** | `BitstreamRange(stream,start,end)` underflow when `end<start` |
| 03 | `bitstream.cc:534` | Low (wrong error, not OOB) | `wait_for_available_bytes` int64 signed overflow |
| 04 | `box.cc:1575` | **High (umbrella)** | iloc field sizes not validated against {0,4,8} |
| 05 | `box.cc:1653` | High | iloc `base_offset_size` not validated |
| 06 | `box.cc:1700` | High | iloc `offset_size` not validated |
| 07 | `box.cc:1711` | High | iloc `length_size` not validated |
| 08 | `box.cc:1857` | Safe (proven) | `extent.offset + base_offset + length` — analysis shows no overflow |
| 09 | `box.cc:1951` | Confirmed | `idat->read_data()` return value silently discarded |
| 10 | `box.cc:1962` | High | `size -= extent.length` integer underflow |
| 11 | `box.cc:4110` | Latent | `m_data_start_pos + get_box_size()` overflow |
| 12 | `box.cc:4116` | Safe (proven) | `start + length > get_box_size()` analysis |
| 13 | `box.cc:4127` | Low | signed/unsigned mix in `wait_for_file_size` arg |
| 14 | `box.cc:4140` | Latent | `istr->seek()` failure caught only by assert |
| 15 | `decoder.cc:96` | Latent | `DataExtent::read_data` OOB iterator advance |
| 16 | `file_layout.cc:105` | Low | `next_box_start + MAXIMUM_BOX_HEADER_SIZE` overflow |
| 17 | `file_layout.cc:143` | **High** | `meta_box_start + box_size` overflow |
| 18 | `file_layout.cc:220` | **High** | `moov_box_start + box_size` overflow |

"High" = reachable from the public API and produces an ASAN-detectable
heap-buffer-overflow.

"Latent" / "Safe (proven)" reproducers ship a harness that **demonstrates the
boundary**: they construct the inputs that *would* trigger if the documented
invariant were ever broken (e.g. via a regression). They are written so a
fuzzer or future refactor that breaks the invariant will surface immediately
under ASAN.

"Low" reproducers ship a UBSAN harness; the bug is signed-overflow UB, not
memory unsafety.

## Building

The harnesses link against a libheif built with `-fsanitize=address,undefined
-DNDEBUG`. (`NDEBUG` is required because several of the bugs are guarded by
`assert()` that aborts before ASAN can observe the OOB.)

```
./build.sh                       # configures + builds with ASAN/UBSAN
./run_all.sh                     # generates inputs, runs every harness
```

`build.sh` requires `cmake` (>=3.16) and a working C++20 compiler. If the host
lacks cmake, install one with `pip install cmake` or download a static binary
from https://cmake.org/download/.

## Running a single reproducer

```
cd build/audit_repros
python3 ../../audit_repros/bug02_bitstreamrange_end_lt_start/gen_poc.py poc.heif
ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 ./bug02 poc.heif
```

For "Latent" / "Safe" bugs the harness drives internal classes directly and
does not need a `.heif` input — see the per-bug README.

## Real sanitizer output

Each `bugNN_*/README.md` ends with a "Real sanitizer output" section showing
the **actual run output** captured by `./build.sh && ./run_all.sh`. Process
IDs, hex addresses, and BuildIds are normalised so the doc diffs cleanly.

What every harness actually produces — captured by `./build.sh && ./run_all.sh`:

| # | Real sanitizer output | Where |
|---|----------------------|-------|
| 01 | UBSAN — `addition of unsigned offset to 0x... overflowed to 0x...` | `bitstream.cc:121` `StreamReader_memory::read` |
| 02 | ASAN heap-buffer-overflow (READ size 1) | `bitstream.cc:121` via `BitstreamRange::read8` after `end < start` underflow |
| 03 | UBSAN — `implicit conversion from type 'unsigned long' of value 9223372036854775863 to 'int64_t' changed the value to -9223372036854775753` | `bitstream.cc:540` |
| 04 | ASAN heap-buffer-overflow (consumer side) | iloc misalignment + bug09 chain |
| 05 | ASAN heap-buffer-overflow (consumer side) | iloc `base_offset_size=2` misalignment |
| 06 | ASAN heap-buffer-overflow (consumer side) | iloc `offset_size=5` misalignment |
| 07 | ASAN heap-buffer-overflow (consumer side) | iloc `length_size=7` misalignment |
| 08 | clean error (watchdog — invariant holds) | reports `extent.offset+base_offset+length = 72057594037927934` proving no overflow |
| 09 | ASAN heap-buffer-overflow (consumer side) | `idat->read_data` error dropped → underfilled `dest` |
| 10 | UBSAN — `unsigned integer overflow: 4 - 4096 cannot be represented` | `box.cc:1969` `size -= extent.length` |
| 11 | ASAN heap-buffer-overflow (READ size 64) | `box.cc:4162` `Box_idat::read_data` reading via `LyingPositionReader` |
| 12 | clean error (watchdog) | `start+length` proven safe |
| 13 | ASAN heap-buffer-overflow (READ size 64) | `box.cc:4162` via `LyingPositionReader` |
| 14 | ASAN heap-buffer-overflow (READ size 64) | `box.cc:4162` via `FailingSeekReader` (assert dropped) |
| 15 | ASAN heap-buffer-overflow (READ size 64) | `decoder.cc:106` `DataExtent::read_data` `m_raw.begin()+offset+size` |
| 16 | clean parse error (watchdog) | bug not reachable via normal box traversal |
| 17 | ASAN heap-buffer-overflow (READ size 1) | `bitstream.cc:121` via `BitstreamRange` ctor with `end<start` |
| 18 | ASAN heap-buffer-overflow (READ size 1) | `bitstream.cc:121` via `BitstreamRange` ctor with `end<start` |

### Summary

**15 of 18** harnesses produce real ASAN or UBSAN reports (rc=1):
bug01, bug02, bug03, bug04, bug05, bug06, bug07, bug09, bug10, bug11,
bug13, bug14, bug15, bug17, bug18.

**3 of 18** are watchdog harnesses that exit cleanly today (the
"safe-by-invariant" cases — bug08, bug12, bug16): they exercise the
boundary so any future regression that breaks the invariant fires the
sanitizer. Their captured logs document what the safe path looks like
right now.

The audit comments overstate severity for the file-layout / idat overflow
bugs (bug02/11/13/14/17/18) because the upstream check
`m_size > MAX_LARGE_BOX_SIZE` (`box.cc:261`) blocks the public-API path —
those harnesses exercise the buggy state directly via internal classes.
The iloc-misalignment bugs (bug04/05/06/07) are silent inside libheif
itself; their harnesses chain through bug09 (idat error dropped) so a
synthetic downstream consumer reproduces a real ASAN OOB.

The audit-comment description for **bug03** is slightly imprecise: the
arithmetic doesn't overflow as signed; the UB is the implicit narrowing
conversion from `uint64_t` to `int64_t`. UBSAN catches it at
`-fsanitize=integer` (the build.sh adds `,integer` to the sanitizer flag
specifically for this case).

## Trust boundary note

These reproducers parse attacker-controlled HEIF/ISOBMFF files. Run them in a
sandbox or VM. None of them exfiltrate data; all I/O is to the supplied path.
