"""Builds designer/inkplate-designer.html from designer/designer.src.html by
inlining the firmware template (firmware/InkplateDisplay/InkplateDisplay.ino and
display_types.h), which the designer puts into every export."""
from pathlib import Path

root = Path(__file__).resolve().parent.parent
src = (root / "designer/designer.src.html").read_text()
ino = (root / "firmware/InkplateDisplay/InkplateDisplay.ino").read_text()
types = (root / "firmware/InkplateDisplay/display_types.h").read_text()
for text in (ino, types):
    assert "</script" not in text.lower()
out = src.replace("@@FW_INO@@", ino.rstrip("\n")).replace("@@FW_TYPES@@", types.rstrip("\n"))
(root / "designer/inkplate-designer.html").write_text(out)
print("wrote designer/inkplate-designer.html,", len(out), "bytes")
