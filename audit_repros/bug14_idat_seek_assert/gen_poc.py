#!/usr/bin/env python3
"""bug14: harness drives Box_idat directly. Placeholder."""
import sys
out = sys.argv[1] if len(sys.argv) > 1 else "bug14_poc.bin"
with open(out, "wb") as f:
    f.write(b"\x00")
print(f"[+] wrote placeholder {out} (bug14 harness ignores it)")
