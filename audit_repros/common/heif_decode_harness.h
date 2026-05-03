// Common bits for the public-API harnesses.
//
// Each public-API bug is reachable by feeding a crafted file to libheif's
// "read from memory + decode" path. This header centralises that code so
// the per-bug harness only has to point at an input file.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <stdint.h>

#include "libheif/heif.h"

static std::vector<uint8_t> slurp(const char* path) {
  FILE* f = std::fopen(path, "rb");
  if (!f) { std::perror(path); std::exit(2); }
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> buf(n > 0 ? static_cast<size_t>(n) : 0);
  if (n > 0 && std::fread(buf.data(), 1, buf.size(), f) != buf.size()) {
    std::perror("fread"); std::exit(2);
  }
  std::fclose(f);
  return buf;
}

// Drives the same path as the OSS-Fuzz file_fuzzer: parse boxes, fetch the
// primary image, decode every top-level image. Bugs that manifest only in
// the decode step (e.g. the iloc data-injection chain) need that final step.
static int run_full_decode(const std::vector<uint8_t>& buf) {
  heif_init(nullptr);

  heif_context* ctx = heif_context_alloc();
  // Match the limits used by the OSS-Fuzz harness so the reproducer is
  // representative of fuzzing conditions.
  auto* limits = heif_context_get_security_limits(ctx);
  limits->max_total_memory = static_cast<uint64_t>(2) * 1024 * 1024 * 1024;
  limits->max_memory_block_size = 128 * 1024 * 1024;

  heif_error err = heif_context_read_from_memory(ctx, buf.data(), buf.size(), nullptr);
  if (err.code != heif_error_Ok) {
    std::fprintf(stderr, "read_from_memory: %s\n", err.message);
    heif_context_free(ctx);
    heif_deinit();
    return 0; // Parse error is acceptable; ASAN finds OOB during the parse itself.
  }

  heif_image_handle* primary = nullptr;
  err = heif_context_get_primary_image_handle(ctx, &primary);
  if (err.code == heif_error_Ok && primary) {
    heif_image* img = nullptr;
    heif_decode_image(primary, &img,
                      heif_colorspace_YCbCr, heif_chroma_420, nullptr);
    if (img) heif_image_release(img);
    heif_image_handle_release(primary);
  }

  int n = heif_context_get_number_of_top_level_images(ctx);
  if (n > 0) {
    std::vector<heif_item_id> ids(static_cast<size_t>(n));
    n = heif_context_get_list_of_top_level_image_IDs(ctx, ids.data(), n);
    for (int i = 0; i < n; ++i) {
      heif_image_handle* h = nullptr;
      if (heif_context_get_image_handle(ctx, ids[i], &h).code == heif_error_Ok && h) {
        heif_image* img = nullptr;
        heif_decode_image(h, &img,
                          heif_colorspace_YCbCr, heif_chroma_420, nullptr);
        if (img) heif_image_release(img);
        heif_image_handle_release(h);
      }
    }
  }

  heif_context_free(ctx);
  heif_deinit();
  return 0;
}
