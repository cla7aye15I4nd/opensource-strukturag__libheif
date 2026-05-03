# Audit-repro PR index

Each `bugNN_*/` directory has its own `PR.md`. Code-change PRs ship
as separate branches **forked from `master`**, each touching a minimal
slice of the tree so they can be reviewed and landed independently.
The `bug08` and `bug12` audit comments are *proven safe*: the audit
text is itself the proof, no code change is needed and they have no
fix branch.

## Severity legend

- **High** — reachable from public API, produces ASAN-detectable
  memory unsafety in libheif itself or a synthetic but realistic
  downstream consumer.
- **Latent** — invariant maintained today; fix hardens against future
  regression.
- **Low** — wrong error rather than memory unsafety (UBSAN-only).
- **Safe (proven)** — no code change needed.

## Branches (all forked from `master`)

| # | Branch | Severity | One-liner | PR doc |
|---|--------|----------|-----------|--------|
| 01 | `fix/streamreader-memory-read-overflow` | Latent | guard `StreamReader_memory::read` against `m_position + size` wrap | [bug01](bug01_streamreader_memory_read_overflow/PR.md) |
| 02 | `fix/bitstreamrange-end-lt-start` | **High** | replace `BitstreamRange` `assert(end >= start)` with a hard error | [bug02](bug02_bitstreamrange_end_lt_start/PR.md) |
| 03 | `fix/wait-for-available-bytes-overflow` | Low | compute `wait_for_available_bytes` target as `uint64_t` with overflow guard | [bug03](bug03_wait_for_available_bytes_signed_overflow/PR.md) |
| 04 | `fix/iloc-field-size-validation` | **High** | reject iloc field sizes outside `{0, 4, 8}` (umbrella) | [bug04](bug04_iloc_field_size_misalignment/PR.md) |
| 05 | `fix/iloc-base-offset-size-validation` | **High** | same check, focused on `base_offset_size` | [bug05](bug05_iloc_base_offset_size/PR.md) |
| 06 | `fix/iloc-offset-size-validation` | **High** | same check, focused on `offset_size` | [bug06](bug06_iloc_offset_size/PR.md) |
| 07 | `fix/iloc-length-size-validation` | **High** | same check, focused on `length_size` | [bug07](bug07_iloc_length_size/PR.md) |
| 08 | — (no branch) | Safe (proven) | proof in audit comment, no code change | [bug08](bug08_iloc_extent_sum_overflow_safe/PR.md) |
| 09 | `fix/iloc-idat-error-propagation` | Confirmed | propagate `idat->read_data()` error in `Box_iloc::read_data` | [bug09](bug09_idat_read_data_ignored/PR.md) |
| 10 | `fix/iloc-size-underflow` | **High** | clamp idat extent read length and saturate `size` accounting | [bug10](bug10_iloc_size_underflow/PR.md) |
| 11 | `fix/idat-data-start-pos-overflow` | Latent | guard `Box_idat::read_data` start/length bounds against overflow | [bug11](bug11_idat_data_start_pos_overflow/PR.md) |
| 12 | — (no branch) | Safe (proven) | proof in audit comment, no code change | [bug12](bug12_idat_start_plus_length_safe/PR.md) |
| 13 | `fix/idat-signed-unsigned-mix` | Low | compute `Box_idat::read_data` `wait_for_file_size` target as `uint64_t` | [bug13](bug13_idat_signed_unsigned_mix/PR.md) |
| 14 | `fix/idat-seek-assert` | Latent | turn `Box_idat::read_data` `seek/read` failures into errors | [bug14](bug14_idat_seek_assert/PR.md) |
| 15 | `fix/dataextent-oob-iter` | Latent | bounds-check `DataExtent::read_data` cached-buffer path | [bug15](bug15_dataextent_oob_iter/PR.md) |
| 16 | `fix/file-layout-header-end-overflow` | Low | saturation guard before `next_box_start + MAXIMUM_BOX_HEADER_SIZE` | [bug16](bug16_file_layout_header_end_overflow/PR.md) |
| 17 | `fix/file-layout-meta-box-overflow` | **High** | reject `meta_box_start + box_size` overflow | [bug17](bug17_file_layout_meta_box_overflow/PR.md) |
| 18 | `fix/file-layout-moov-box-overflow` | **High** | reject `moov_box_start + box_size` overflow | [bug18](bug18_file_layout_moov_box_overflow/PR.md) |

All branches were verified by running the matching harness from
`audit_repros/run_all.sh` against an ASAN/UBSAN build.

## Branch dependencies

The branches are independent in terms of files and lines, but a few
pairs share the same call graph. Reviewers should know:

- **bug02 ⟷ bug17/bug18.** bug02 makes the `BitstreamRange`
  constructor itself robust to `end < start`; bug17/bug18 cut off the
  overflow in the meta/moov branches before `BitstreamRange` is
  constructed. Either alone fixes the reachable-from-public-API
  attack; landing both is defence in depth.
- **bug04 ⟷ bug05/06/07.** All four ship the same `{0, 4, 8}`
  validation block. Any one PR closes the other three; they are
  separated for traceability against the audit comments. If a single
  bundled PR is preferred, use `fix/iloc-field-size-validation`.
- **bug09 ⟷ bug10.** Both touch `Box_iloc::read_data`'s idat branch,
  on adjacent lines.
- **bug11 ⟷ bug13/bug14.** All three are inside `Box_idat::read_data`
  on consecutive blocks.

## Conventional sequence

If landing all branches, a low-risk sequence is:

1. Latent / low: `fix/streamreader-memory-read-overflow`,
   `fix/wait-for-available-bytes-overflow`,
   `fix/idat-data-start-pos-overflow`,
   `fix/idat-signed-unsigned-mix`,
   `fix/idat-seek-assert`,
   `fix/dataextent-oob-iter`,
   `fix/file-layout-header-end-overflow`.
2. High but isolated: `fix/iloc-field-size-validation` (or per-nibble
   alternatives), `fix/iloc-idat-error-propagation`,
   `fix/iloc-size-underflow`.
3. High and related: `fix/bitstreamrange-end-lt-start`, then
   `fix/file-layout-meta-box-overflow`, then
   `fix/file-layout-moov-box-overflow`.
