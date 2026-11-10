"""Builds the designers from designer/designer.src.html, one per board, by
filling in the board settings and inlining that board's firmware template
(the .ino and display_types.h), which the designer puts into every export.

  designer/inkplate-designer.html   Inkplate 6COLOR  (firmware/InkplateDisplay)
  designer/inkplate6-designer.html  Inkplate 6, black and white  (firmware/Inkplate6Display)
"""
import json
from pathlib import Path

root = Path(__file__).resolve().parent.parent
src = (root / "designer/designer.src.html").read_text()
BOARDS = [
    ("inkplate-designer.html", {"id": "color", "name": "Inkplate 6COLOR", "W": 600, "H": 448, "bw": False,
                                "sketch": "InkplateDisplay", "board": "Soldered Inkplate 6COLOR", "store": "ip6", "zip": "inkplate-display.zip"}),
    ("inkplate6-designer.html", {"id": "bw", "name": "Inkplate 6", "W": 800, "H": 600, "bw": True,
                                 "sketch": "Inkplate6Display", "board": "Soldered Inkplate6", "store": "ip6bw", "zip": "inkplate6-display.zip"}),
]
for out_name, board in BOARDS:
    fw = root / "firmware" / board["sketch"]
    ino = (fw / (board["sketch"] + ".ino")).read_text()
    types = (fw / "display_types.h").read_text()
    for text in (ino, types):
        assert "</script" not in text.lower()
    out = (src.replace("@@BOARD@@", json.dumps(board))
              .replace("@@FW_INO@@", ino.rstrip("\n")).replace("@@FW_TYPES@@", types.rstrip("\n")))
    if board["bw"]:
        out = out.replace("<title>Inkplate 6COLOR Designer</title>", "<title>Inkplate 6 Designer</title>")
    (root / "designer" / out_name).write_text(out)
    print("wrote designer/" + out_name + ",", len(out), "bytes")
