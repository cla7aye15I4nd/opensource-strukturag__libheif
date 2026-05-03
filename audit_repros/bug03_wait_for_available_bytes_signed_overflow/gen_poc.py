#!/usr/bin/env python3
"""bug03: UBSAN harness, no input file."""
import sys
out = sys.argv[1] if len(sys.argv) > 1 else "bug03_poc.bin"
with open(out, "wb") as f:
    f.write(b"\x00")
print(f"[+] wrote placeholder {out} (bug03 harness ignores it)")
