#!/usr/bin/env python3
"""bug02 PoC: same as bug17 — exercises the BitstreamRange ctor underflow
via the file_layout meta-box overflow."""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "bug17_file_layout_meta_box_overflow"))
from gen_poc import main
if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "bug02_poc.heif")
