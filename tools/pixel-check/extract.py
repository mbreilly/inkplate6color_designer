"""Copies the drawing code out of a firmware sketch so harness.cpp can run it on a PC.

usage: extract.py [sketch]   (InkplateDisplay by default, or Inkplate6Display)
"""
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
sketch = sys.argv[1] if len(sys.argv) > 1 else "InkplateDisplay"
s = (here / "../../firmware" / sketch / (sketch + ".ino")).read_text()
a = s.index("// ---------------------------------------------------------------- text")
b = s.index("bool drawImageFile(")
b = s.rindex("}\n", 0, b) + 2  # back up over its comment
c = s.index("void drawMain(")
d = s.index("// Returns false if the photo")
(here / "draw_part.inc").write_text(s[a:b] + s[c:d])
