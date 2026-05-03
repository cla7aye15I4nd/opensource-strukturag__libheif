#!/usr/bin/env python3
"""bug01 has no on-disk PoC — drives StreamReader_memory directly. Placeholder."""
import sys
out = sys.argv[1] if len(sys.argv) > 1 else "bug01_poc.bin"
with open(out, "wb") as f:
    f.write(b"\x00")
print(f"[+] wrote placeholder {out} (bug01 harness ignores it)")
