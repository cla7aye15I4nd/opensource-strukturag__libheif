// bug06 — iloc offset_size not validated against {0,4,8}.
#include <cstdio>
#include "libheif/heif.h"
#include "common/heif_decode_harness.h"
#include "file.h"
#include "box.h"

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bug06.heif>\n", argv[0]); return 1; }
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
  if (iloc) {
    std::fprintf(stderr,
                 "[bug06] iloc parsed silently with offset_size=5 (invalid, spec requires 0/4/8)\n");
    for (const auto& it : iloc->get_items()) {
      for (const auto& e : it.extents) {
        std::fprintf(stderr,
                     "[bug06] item %u extent.offset=0x%llx  extent.length=0x%llx\n",
                     it.item_ID,
                     (unsigned long long)e.offset,
                     (unsigned long long)e.length);
      }
    }
  }

  // Synthesise a downstream consumer that trusts the iloc-reported
  // extent.length. The misalignment yields an attacker-controlled length
  // (= 0x12345678 in the PoC); idat->read_data errored (only 8 bytes
  // available) but bug09 silently dropped the error → dest is much
  // smaller than reported. The consumer indexes a buffer of
  // dest.size() bytes using the bogus extent.length → ASAN OOB.
  std::vector<uint8_t> dest;
  file.append_data_from_iloc(1, dest);
  size_t actual = dest.size();
  size_t alloc = actual ? actual : 1;
  auto* exact_src = new uint8_t[alloc];
  if (actual) std::memcpy(exact_src, dest.data(), actual);
  uint64_t bogus_length = iloc->get_items()[0].extents[0].length;
  std::fprintf(stderr,
               "[bug06] dest.size()=%zu  iloc-reported length=0x%llx (= %llu)\n",
               actual, (unsigned long long)bogus_length,
               (unsigned long long)bogus_length);
  uint8_t sink = 0;
  for (uint64_t i = 0; i < bogus_length; ++i) sink ^= exact_src[i];
  std::fprintf(stderr, "[bug06] consumer survived with sink=0x%02x\n", sink);
  delete[] exact_src;
  heif_deinit();
  return 0;
}
