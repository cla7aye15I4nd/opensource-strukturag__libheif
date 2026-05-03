#!/usr/bin/env python3
"""bug11: harness drives Box_idat directly. Placeholder file."""
import sys
out = sys.argv[1] if len(sys.argv) > 1 else "bug11_poc.bin"
with open(out, "wb") as f:
    f.write(b"\x00")
print(f"[+] wrote placeholder {out} (bug11 harness ignores it)")
