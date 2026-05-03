// bug17 — meta_box_start + get_box_size() unsigned overflow.
//
// The public-API path that *would* trigger this (a meta box with a
// crafted largesize) is mitigated by the MAX_LARGE_BOX_SIZE check at
// box.cc:261, which rejects any box-size > 0x0FFFFFFFFFFFFFFF before it
// reaches FileLayout::read at file_layout.cc:150.  This harness performs
// the audit-described overflow directly, exposing the consequence the
// audit comment names: m_remaining = ~SIZE_MAX, then unbounded reads.
//
// Setup:
//   - real backing buffer: 64 bytes on the heap
//   - StreamReader_memory claims 1 GiB ("future regression that lifts
//     MAX_LARGE_BOX_SIZE" / "non-memory reader that doesn't bounds-check")
//   - directly compute end_of_meta_box = meta_box_start + box_size
//     where box_size wraps the addition past UINT64_MAX
//   - construct BitstreamRange(stream, meta_box_start, end_of_meta_box)
//   - read past the real allocation → ASAN heap-buffer-overflow

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "bitstream.h"

int main() {
  std::vector<uint8_t> real_buf(64, 0xCD);
  uint64_t fake_length = static_cast<uint64_t>(1) << 30;  // 1 GiB lie
  auto reader = std::make_shared<StreamReader_memory>(real_buf.data(),
                                                      (size_t)fake_length, false);

  // Mirror the file_layout.cc:150 expression:
  //   end_of_meta_box = meta_box_start + box_header.get_box_size();
  uint64_t meta_box_start = 24;                                   // small file pos
  uint64_t crafted_box_size = static_cast<uint64_t>(-1) - 19;     // UINT64_MAX-19
  uint64_t end_of_meta_box = meta_box_start + crafted_box_size;   // wraps to 4
  std::fprintf(stderr,
               "[bug17] meta_box_start=%llu  crafted_box_size=0x%016llx  "
               "end_of_meta_box (wrapped)=%llu\n",
               (unsigned long long)meta_box_start,
               (unsigned long long)crafted_box_size,
               (unsigned long long)end_of_meta_box);

  // Same call as file_layout.cc:161
  BitstreamRange r(reader, (size_t)meta_box_start, (size_t)end_of_meta_box);
  // m_remaining = end - start = 4 - 24 = -20 → underflow to ~SIZE_MAX.

  uint8_t sink = 0;
  for (int i = 0; i < 256; ++i) sink ^= r.read8();
  std::fprintf(stderr, "[bug17] survived OOB walk, sink=0x%02x\n", sink);
  return 0;
}
