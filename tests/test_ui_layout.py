"""UI regression checks after overlapping controls were reported on hardware."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class LayoutTests(unittest.TestCase):
    def test_single_controls_hint_and_footer_status(self):
        bg = (ROOT / "native/design/render.py").read_text()
        c = (ROOT / "native/source/main.c").read_text()
        self.assertIn("glass((64,621,1217,708)", bg)
        self.assertIn("L/R  Sections", bg)
        self.assertNotIn("A / X  Apple ID login via Chromium", bg)
        self.assertNotIn("L / R  Sections   Up / Down", bg)
        self.assertNotIn("label((952,495),'Connect Apple Music'", bg)
        self.assertNotIn('draw_text(pixels,stride,742,570', c)
        self.assertIn("draw_text(pixels,stride,95,663", c)
        self.assertIn("draw_text(pixels,stride,822,499", c)

if __name__ == "__main__":
    unittest.main()
