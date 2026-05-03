// bug08 — extent.offset + base_offset + length addition (proven safe).
// UBSAN watchdog harness. Reads the input via the public API; success means
// the addition completed without overflow (the audit comment's invariant
// holds today).
#include <cstdio>
#include "common/heif_decode_harness.h"
#include "file.h"

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: %s <bug08.heif>\n", argv[0]); return 1; }
  auto buf = slurp(argv[1]);
  heif_init(nullptr);
  HeifFile file;
  static heif_security_limits limits = *heif_get_global_security_limits();
  limits.max_total_memory = static_cast<uint64_t>(2) * 1024 * 1024 * 1024;
  limits.max_memory_block_size = 128 * 1024 * 1024;
  file.set_security_limits(&limits);

  Error err = file.read_from_memory(buf.data(), buf.size(), false);
  std::fprintf(stderr, "[bug08] read_from_memory err=\"%s\"\n", err.message.c_str());

  // Force the read_data path (which contains the audited addition).
  std::vector<uint8_t> dest;
  Error e = file.append_data_from_iloc(1, dest);
  std::fprintf(stderr,
               "[bug08] append_data_from_iloc(id=1) err=\"%s\" "
               "(UBSAN must NOT have fired on extent.offset+base_offset+length)\n",
               e.message.c_str());
  heif_deinit();
  return 0;
}
