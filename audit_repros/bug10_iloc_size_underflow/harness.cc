// bug10 — `size -= extent.length` underflow in Box_iloc::read_data when
// extent.length > caller-requested size. Drives HeifFile::append_data_from_iloc
// with a small `size` to expose the underflow on the construction_method=1
// (idat) branch.

#include <cstdio>
#include <vector>
#include <fstream>
#include <sstream>

#include "common/heif_decode_harness.h"
#include "file.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <bug10.heif>\n", argv[0]);
    return 1;
  }
  auto buf = slurp(argv[1]);

  heif_init(nullptr);
  HeifFile file;
  static heif_security_limits limits = *heif_get_global_security_limits();
  limits.max_total_memory = static_cast<uint64_t>(2) * 1024 * 1024 * 1024;
  limits.max_memory_block_size = 128 * 1024 * 1024;
  file.set_security_limits(&limits);

  Error err = file.read_from_memory(buf.data(), buf.size(), false);
  std::fprintf(stderr, "[bug10] HeifFile::read_from_memory: %s\n",
               err.message.c_str());
  if (err) { heif_deinit(); return 0; }

  auto ids = file.get_item_IDs();
  for (auto id : ids) {
    std::vector<uint8_t> dest;
    // Request exactly 4 bytes. extent[0] reports length 0x1000.
    // Box_iloc::read_data: size -= extent.length → 4 - 0x1000 wraps.
    // On extent[1] (length 0x10), the construction_method=1 branch reads
    // all 0x10 bytes regardless of `size`, then `size -= 0x10` wraps again.
    Error e = file.append_data_from_iloc(id, dest, /*offset=*/0, /*size=*/4);
    std::fprintf(stderr,
                 "[bug10] append_data_from_iloc(id=%u, size=4): err=\"%s\"  bytes_read=%zu\n",
                 id, e.message.c_str(), dest.size());

    // Synthesise a "well-behaved" caller that trusted the size contract:
    // it allocated exactly `size` bytes and copies from `dest` assuming
    // dest.size() <= size. With the underflow, dest.size() == 4096 → OOB
    // write into the 4-byte buffer → ASAN heap-buffer-overflow.
    if (dest.size() > 4) {
      auto* caller_buf = new uint8_t[4];
      std::memcpy(caller_buf, dest.data(), dest.size());   // ASAN fires here
      delete[] caller_buf;
    }
  }

  heif_deinit();
  return 0;
}
