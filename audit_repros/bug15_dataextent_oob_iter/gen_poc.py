#!/usr/bin/env python3
"""bug15: harness drives DataExtent directly. Placeholder."""
import sys
out = sys.argv[1] if len(sys.argv) > 1 else "bug15_poc.bin"
with open(out, "wb") as f:
    f.write(b"\x00")
print(f"[+] wrote placeholder {out} (bug15 harness ignores it)")
