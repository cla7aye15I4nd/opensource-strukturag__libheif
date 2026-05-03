// bug09 — idat->read_data() return value silently discarded.
//
// Drives HeifFile::append_data_from_iloc directly. With the bug, the call
// returns Error::Ok despite the underlying idat read failing — observable
// as: extent declared N bytes, dest contains 0 bytes, error code is Ok.

#include <cstdio>
#include <vector>

#include "libheif/heif.h"
#include "common/heif_decode_harness.h"
#include "file.h"
#include "box.h"

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bug09.heif>\n", argv[0]); return 1; }
  auto buf = slurp(argv[1]);

  heif_init(nullptr);
  HeifFile file;
  static heif_security_limits limits = *heif_get_global_security_limits();
  limits.max_total_memory = static_cast<uint64_t>(2) * 1024 * 1024 * 1024;
  limits.max_memory_block_size = 128 * 1024 * 1024;
  file.set_security_limits(&limits);

  Error err = file.read_from_memory(buf.data(), buf.size(), false);
  std::fprintf(stderr, "read_from_memory: code=%d msg=%s\n",
               (int)err.error_code, err.message.c_str());

  auto meta = file.get_meta_box();
  auto iloc = meta ? meta->get_child_box<Box_iloc>() : nullptr;
  if (!iloc) { heif_deinit(); return 0; }

  for (const auto& item : iloc->get_items()) {
    uint64_t expected_total = 0;
    for (const auto& e : item.extents) expected_total += e.length;

    std::vector<uint8_t> dest;
    Error e = file.append_data_from_iloc(item.item_ID, dest);
    std::fprintf(stderr,
                 "[bug09] item %u: extent_total=%llu  bytes_read=%zu  err.code=%d msg=\"%s\"\n",
                 item.item_ID,
                 (unsigned long long)expected_total,
                 dest.size(),
                 (int)e.error_code, e.message.c_str());
    if (e.error_code == 0 && dest.size() < expected_total) {
      std::fprintf(stderr,
                   "[bug09] >>> idat read silently failed: extent declared %llu bytes,\n"
                   "        dest contains %zu bytes, but Box_iloc::read_data returned Ok\n",
                   (unsigned long long)expected_total, dest.size());

      // Synthesise a "well-behaved" caller that trusted the no-error
      // return: it allocates a 16-byte heap region (matching dest.size())
      // and indexes into it using the iloc-reported extent_total. Past
      // dest.size(), every byte read OOBs → ASAN heap-buffer-overflow.
      auto* exact_src = new uint8_t[dest.size()];
      std::memcpy(exact_src, dest.data(), dest.size());
      uint8_t sink = 0;
      for (uint64_t i = 0; i < expected_total; ++i) {
        sink ^= exact_src[i];   // ASAN fires the moment i >= dest.size()
      }
      std::fprintf(stderr, "[bug09] sink=0x%02x\n", sink);
      delete[] exact_src;
    }
  }

  heif_deinit();
  return 0;
}
