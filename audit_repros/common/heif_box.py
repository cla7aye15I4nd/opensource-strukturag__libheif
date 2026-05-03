"""Minimal HEIF/ISOBMFF box construction utilities.

Just enough to build malformed inputs for the audit reproducers — not a full
ISOBMFF writer. Each helper returns raw bytes; PoC generators glue them
together.
"""

import struct


def box(box_type: bytes, payload: bytes, force_largesize: bool = False,
        largesize_override: int | None = None) -> bytes:
    """Wrap `payload` in an ISOBMFF box.

    If `largesize_override` is set, a 64-bit largesize field is written with
    that exact value (used by the file_layout overflow PoCs to make
    `meta_box_start + box_size` wrap).
    """
    assert len(box_type) == 4
    if largesize_override is not None:
        # size=1 → read largesize field; we put the attacker-chosen 64-bit value there.
        return (struct.pack(">I", 1) + box_type +
                struct.pack(">Q", largesize_override) + payload)
    size = 8 + len(payload)
    if size >= 0xFFFFFFFF or force_largesize:
        return (struct.pack(">I", 1) + box_type +
                struct.pack(">Q", 16 + len(payload)) + payload)
    return struct.pack(">I", size) + box_type + payload


def fullbox(box_type: bytes, version: int, flags: int, payload: bytes) -> bytes:
    return box(box_type, struct.pack(">B", version) + struct.pack(">I", flags)[1:] + payload)


def ftyp(major=b"heic", minor=0, brands=(b"mif1", b"heic")) -> bytes:
    p = major + struct.pack(">I", minor)
    for b in brands:
        p += b
    return box(b"ftyp", p)


def hdlr(handler_type=b"pict") -> bytes:
    p = (struct.pack(">I", 0) +              # pre_defined
         handler_type +                       # handler_type
         b"\x00" * 12 +                       # reserved
         b"libheif\x00")                      # name (cstring)
    return fullbox(b"hdlr", 0, 0, p)


def pitm(item_id: int, version: int = 0) -> bytes:
    if version == 0:
        return fullbox(b"pitm", 0, 0, struct.pack(">H", item_id))
    return fullbox(b"pitm", 1, 0, struct.pack(">I", item_id))


def iinf(items) -> bytes:
    """Build an iinf with the given list of (item_id, item_type) tuples."""
    p = struct.pack(">H", len(items))
    for item_id, item_type in items:
        # infe v2: id, protection_index, item_type, item_name(""), ...
        infe_payload = (
            struct.pack(">H", item_id) +
            struct.pack(">H", 0) +
            item_type +
            b"\x00"
        )
        p += fullbox(b"infe", 2, 0, infe_payload)
    return fullbox(b"iinf", 0, 0, p)


def iloc(version: int, offset_size: int, length_size: int,
         base_offset_size: int, index_size: int,
         items) -> bytes:
    """Build an iloc box with explicit field-size nibbles.

    `items` is a list of dicts:
       {item_id, construction_method (v1/v2 only), data_reference_index,
        base_offset, extents=[(index, offset, length), ...]}

    The field-size validation bug PoCs rely on this taking *invalid* values
    (1–3, 5–7, 9–15) for the size nibbles.
    """
    assert 0 <= offset_size <= 15
    assert 0 <= length_size <= 15
    assert 0 <= base_offset_size <= 15
    assert 0 <= index_size <= 15

    nibbles = (offset_size << 12) | (length_size << 8) | (base_offset_size << 4)
    if version >= 1:
        nibbles |= index_size
    p = struct.pack(">H", nibbles)

    if version < 2:
        p += struct.pack(">H", len(items))
    else:
        p += struct.pack(">I", len(items))

    def emit_uint(value: int, byte_size: int) -> bytes:
        # Used for the {0,4,8} branches *and* the deliberately-bogus sizes —
        # writes exactly `byte_size` bytes for emit purposes; readers may
        # silently skip when byte_size is not in {0,4,8}.
        if byte_size == 0:
            return b""
        # Mask to the requested width so PoC authors can write arbitrary
        # 64-bit values regardless of the bogus nibble — only the low
        # byte_size bytes survive.
        masked = value & ((1 << (byte_size * 8)) - 1)
        return masked.to_bytes(byte_size, "big")

    for it in items:
        if version < 2:
            p += struct.pack(">H", it["item_id"])
        else:
            p += struct.pack(">I", it["item_id"])

        if version >= 1:
            p += struct.pack(">H", it.get("construction_method", 0))

        p += struct.pack(">H", it.get("data_reference_index", 0))
        p += emit_uint(it.get("base_offset", 0), base_offset_size)

        p += struct.pack(">H", len(it["extents"]))
        for (idx, off, ln) in it["extents"]:
            if version >= 1 and index_size > 0:
                p += emit_uint(idx, index_size)
            p += emit_uint(off, offset_size)
            p += emit_uint(ln, length_size)

    return fullbox(b"iloc", version, 0, p)


def idat(payload: bytes) -> bytes:
    return box(b"idat", payload)


def empty_iprp() -> bytes:
    """Minimal iprp containing an empty ipco and a near-empty ipma —
    enough for HeifFile::parse_heif_file() to clear the
    `No_iprp_box`/`No_ipco_box`/`No_ipma_box` validation gates and reach
    the iloc/idat code paths."""
    ipco = box(b"ipco", b"")
    # ipma v0 with 0 entries (entry_count = 0).
    ipma = fullbox(b"ipma", 0, 0, struct.pack(">I", 0))
    return box(b"iprp", ipco + ipma)


def meta(handler_type: bytes, sub_boxes_payload: bytes,
         largesize_override: int | None = None) -> bytes:
    inner = fullbox(b"meta", 0, 0,
                    hdlr(handler_type) + sub_boxes_payload)
    # `meta` itself is a fullbox; the helper above already wraps it, but we
    # want to be able to override the largesize field for bug17/bug18 PoCs.
    if largesize_override is None:
        return inner
    # Re-wrap with a controlled largesize. Strip the existing 8-byte header,
    # then re-emit with a 16-byte largesize header pointing wherever caller
    # wants.
    body = inner[8:]
    return (struct.pack(">I", 1) + b"meta" +
            struct.pack(">Q", largesize_override) + body)


def moov(payload: bytes, largesize_override: int | None = None) -> bytes:
    if largesize_override is None:
        return box(b"moov", payload)
    return (struct.pack(">I", 1) + b"moov" +
            struct.pack(">Q", largesize_override) + payload)


def write_file(path: str, data: bytes) -> None:
    with open(path, "wb") as f:
        f.write(data)
    print(f"[+] wrote {path} ({len(data)} bytes)")
