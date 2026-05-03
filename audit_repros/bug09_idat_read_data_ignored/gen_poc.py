#!/usr/bin/env python3
"""bug09: iloc construction_method=1 with extent.offset+length > idat size.
idat->read_data returns End_of_data; Box_iloc::read_data ignores it."""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "common"))
import heif_box as hb


def main(out_path: str) -> None:
    idat_payload = b"AB" * 8             # 16 bytes of idat
    iloc_box = hb.iloc(
        version=1,
        offset_size=4, length_size=4,
        base_offset_size=0, index_size=0,
        items=[{
            "item_id": 1,
            "construction_method": 1,    # data is inside idat
            "data_reference_index": 0,
            # Two extents:
            #   ext 1: 16 bytes from idat[0..16]   (succeeds)
            #   ext 2: 4080 bytes from idat[16..]  (idat is only 16 bytes →
            #          idat->read_data returns End_of_data)
            # Because Box_iloc::read_data discards that error, the function
            # returns Ok with `dest` containing only 16 bytes — but iloc
            # *says* the item has 16+4080 = 4096 bytes.
            "extents": [(0, 0, 16), (0, 16, 4080)],
        }])

    payload = (hb.hdlr(b"pict") +
               hb.pitm(1) +
               hb.empty_iprp() +
               hb.iinf([(1, b"hvc1")]) +
               iloc_box +
               hb.idat(idat_payload))

    out = hb.ftyp() + hb.fullbox(b"meta", 0, 0, payload)
    hb.write_file(out_path, out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "bug09_poc.heif")
