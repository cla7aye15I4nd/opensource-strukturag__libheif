#!/usr/bin/env python3
"""bug16 has no on-disk PoC — the harness drives a custom heif_reader
directly. This script is provided for symmetry with the other bugs and
emits a 1-byte placeholder so 'run all' loops don't bail."""

import sys, os
out = sys.argv[1] if len(sys.argv) > 1 else "bug16_poc.bin"
with open(out, "wb") as f:
    f.write(b"\x00")
print(f"[+] wrote placeholder {out} (bug16 harness ignores it)")
