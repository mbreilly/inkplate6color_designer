"""Copies the drawing code out of the firmware so harness.cpp can run it on a PC."""
from pathlib import Path

here = Path(__file__).resolve().parent
s = (here / "../../firmware/InkplateDisplay/InkplateDisplay.ino").read_text()
a = s.index("// ---------------------------------------------------------------- text")
b = s.index("// Draws a 600x448 image from the SD card")
c = s.index("void drawMain(")
d = s.index("// Returns false if the photo")
(here / "draw_part.inc").write_text(s[a:b] + s[c:d])
