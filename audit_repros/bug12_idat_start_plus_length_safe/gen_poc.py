#!/usr/bin/env python3
"""bug12: UBSAN watchdog harness, no input file."""
import sys
out = sys.argv[1] if len(sys.argv) > 1 else "bug12_poc.bin"
with open(out, "wb") as f:
    f.write(b"\x00")
print(f"[+] wrote placeholder {out} (bug12 harness ignores it)")
