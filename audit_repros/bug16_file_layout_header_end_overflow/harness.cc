// bug16 — next_box_start + MAXIMUM_BOX_HEADER_SIZE overflow check.
//
// This is a "boundary contract" reproducer. The current code is safe because
// next_box_start is bounded by m_max_length (a size_t derived from the input
// buffer). We demonstrate the boundary by walking next_box_start within
// MAXIMUM_BOX_HEADER_SIZE of UINT64_MAX using a custom heif_reader that
// claims an attacker-friendly stream length but never delivers the bytes.
//
// On the *current* code this exits with a clean parse error. If a future
// refactor removes the saturation guard (or someone adds a 64-bit-aware path
// that lets next_box_start escape the m_max_length cap), ASAN/UBSAN will
// flag the integer overflow.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include "libheif/heif.h"

struct LiarReader {
  uint64_t pos = 0;
};

static int64_t lr_get_position(void* u) {
  return (int64_t)((LiarReader*)u)->pos;
}
static int lr_read(void* data, size_t size, void* u) {
  // Refuse all reads — the bug is in arithmetic before any read.
  std::memset(data, 0, size);
  return 1;
}
static int lr_seek(int64_t pos, void* u) {
  ((LiarReader*)u)->pos = (uint64_t)pos;
  return 0;
}
static heif_reader_grow_status lr_wait_for_file_size(int64_t target, void* u) {
  // Claim every requested size (no matter how absurd) is "reached" — this is
  // exactly the case that, combined with a non-saturating addition, would
  // let next_box_start overflow.
  return heif_reader_grow_status_size_reached;
}

int main() {
  heif_init(nullptr);

  static heif_reader r{};
  r.reader_api_version = 1;
  r.get_position = lr_get_position;
  r.read = lr_read;
  r.seek = lr_seek;
  r.wait_for_file_size = lr_wait_for_file_size;

  LiarReader user;
  heif_context* ctx = heif_context_alloc();
  heif_error err = heif_context_read_from_reader(ctx, &r, &user, nullptr);
  std::fprintf(stderr, "read_from_reader err: code=%d sub=%d msg=%s\n",
               err.code, err.subcode, err.message ? err.message : "(null)");

  heif_context_free(ctx);
  heif_deinit();

  // We expect: clean parse error (this is the "in practice safe" branch).
  // ASAN flags any actual overflow if the safeguard regresses.
  return 0;
}
