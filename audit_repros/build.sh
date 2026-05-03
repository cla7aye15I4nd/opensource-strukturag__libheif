#!/usr/bin/env bash
# Builds libheif with ASAN+UBSAN and the audit reproducer harnesses.
#
# NDEBUG is defined so the assert()s that guard several of the audited bugs
# are compiled out — otherwise the program aborts in the assert before ASAN
# can observe the out-of-bounds read.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build-audit-asan"

CFLAGS_SAN="-fsanitize=address,undefined,integer -fno-omit-frame-pointer -fno-sanitize-recover=all -g -O1 -DNDEBUG"

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake "${REPO_ROOT}" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_C_COMPILER="${CC:-clang}" \
  -DCMAKE_CXX_COMPILER="${CXX:-clang++}" \
  -DCMAKE_C_FLAGS="${CFLAGS_SAN}" \
  -DCMAKE_CXX_FLAGS="${CFLAGS_SAN}" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined,integer" \
  -DWITH_EXAMPLES=OFF \
  -DWITH_GDK_PIXBUF=OFF \
  -DWITH_LIBSHARPYUV=OFF \
  -DWITH_AOM_DECODER=OFF -DWITH_AOM_ENCODER=OFF \
  -DWITH_X265=OFF -DWITH_LIBDE265=OFF \
  -DWITH_DAV1D=OFF -DWITH_RAV1E=OFF -DWITH_SvtEnc=OFF \
  -DWITH_JPEG_DECODER=OFF -DWITH_JPEG_ENCODER=OFF \
  -DWITH_OpenJPEG_DECODER=OFF -DWITH_OpenJPEG_ENCODER=OFF \
  -DWITH_OpenH264_DECODER=OFF -DWITH_OpenH264_ENCODER=OFF \
  -DWITH_UVG266=OFF -DWITH_KVAZAAR=OFF \
  -DWITH_FFMPEG_DECODER=OFF \
  -DWITH_HEADER_COMPRESSION=OFF \
  -DBUILD_TESTING=OFF \
  -DENABLE_AUDIT_REPROS=ON

cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo
echo "Built reproducers:"
ls "${BUILD_DIR}/audit_repros/" | grep -E '^bug' || true
