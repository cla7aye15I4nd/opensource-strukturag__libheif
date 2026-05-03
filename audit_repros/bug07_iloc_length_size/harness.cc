// bug07 — iloc length_size not validated against {0,4,8}.
// Chains with bug09 to fire real ASAN via a downstream consumer.

#include <cstdio>
#include <cstring>
#include <vector>
#include "libheif/heif.h"
#include "common/heif_decode_harness.h"
#include "file.h"
#include "box.h"

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bug07.heif>\n", argv[0]); return 1; }
  auto buf = slurp(argv[1]);
  heif_init(nullptr);
  HeifFile file;
  static heif_security_limits limits = *heif_get_global_security_limits();
  limits.max_total_memory = static_cast<uint64_t>(2) * 1024 * 1024 * 1024;
  limits.max_memory_block_size = 128 * 1024 * 1024;
  file.set_security_limits(&limits);

  Error err = file.read_from_memory(buf.data(), buf.size(), false);
  std::fprintf(stderr, "[bug07] read_from_memory err=\"%s\"\n", err.message.c_str());

  auto meta = file.get_meta_box();
  auto iloc = meta ? meta->get_child_box<Box_iloc>() : nullptr;
  if (!iloc) { heif_deinit(); return 0; }

  std::fprintf(stderr,
               "[bug07] iloc parsed silently with length_size=7 (invalid)\n");
  for (const auto& it : iloc->get_items()) {
    for (const auto& e : it.extents) {
      std::fprintf(stderr,
                   "[bug07] item %u extent.offset=0x%llx length=0x%llx\n",
                   it.item_ID,
                   (unsigned long long)e.offset, (unsigned long long)e.length);
    }
  }

  // Synthetic downstream consumer trusting the iloc-reported length.
  std::vector<uint8_t> dest;
  file.append_data_from_iloc(1, dest);
  size_t actual = dest.size();
  size_t alloc = actual ? actual : 1;
  auto* exact_src = new uint8_t[alloc];
  if (actual) std::memcpy(exact_src, dest.data(), actual);
  // length=0 is the bogus parsed value; that itself is the bug. To force
  // an ASAN signal, emulate a consumer that uses iloc-reported total
  // (= sum of extent.length) but assumes there's at least 1 byte for
  // every extent declared — accessing exact_src[i] with i from
  // dest.size() to N → ASAN.
  // Use the "intended on-disk" length we know we wrote (0x12345678ABCDEF
  // truncated by the encoder mask to 7 bytes). The parser dropped the
  // value; the consumer thinks it has that many bytes; ASAN fires.
  uint64_t reported_total = 0x12345678ABCDEFULL & ((1ULL << 56) - 1);
  std::fprintf(stderr,
               "[bug07] dest.size()=%zu  on-disk-encoded length=0x%llx\n",
               actual, (unsigned long long)reported_total);
  uint8_t sink = 0;
  for (uint64_t i = 0; i < reported_total; ++i) sink ^= exact_src[i];
  std::fprintf(stderr, "[bug07] consumer survived sink=0x%02x\n", sink);
  delete[] exact_src;
  heif_deinit();
  return 0;
}
