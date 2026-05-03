#!/usr/bin/env python3
"""bug13: UBSAN harness, no input file."""
import sys
out = sys.argv[1] if len(sys.argv) > 1 else "bug13_poc.bin"
with open(out, "wb") as f:
    f.write(b"\x00")
print(f"[+] wrote placeholder {out} (bug13 harness ignores it)")
