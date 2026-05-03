#!/usr/bin/env python3
"""bug08: iloc with each pointer at MAX_FILE_POS — exercises the upper bound
of the addition documented as overflow-safe."""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "common"))
import heif_box as hb

MAX_FILE_POS = (1 << 55) - 1


def main(out_path: str) -> None:
    bogus_iloc = hb.iloc(
        version=1,
        offset_size=8, length_size=8, base_offset_size=8, index_size=0,
        items=[{
            "item_id": 1, "construction_method": 0, "data_reference_index": 0,
            "base_offset": MAX_FILE_POS,
            "extents": [(0, MAX_FILE_POS, MAX_FILE_POS)],
        }])

    payload = (hb.hdlr(b"pict") + hb.pitm(1) +
               hb.empty_iprp() + hb.iinf([(1, b"hvc1")]) + bogus_iloc)
    out = hb.ftyp() + hb.fullbox(b"meta", 0, 0, payload)
    hb.write_file(out_path, out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "bug08_poc.heif")
