"""Regression tests for Turnwave NX2 v0.6 layout and clean builds."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class LayoutTests(unittest.TestCase):
    def test_separate_ui_zones_and_fresh_binary(self):
        bg=(ROOT/'native/design/render.py').read_text()
        source=(ROOT/'native/source/main.c').read_text()
        build=(ROOT/'build-native.sh').read_text()
        make=(ROOT/'native/Makefile').read_text()
        self.assertIn("BUILD 0.6.0",bg)
        self.assertIn("VERSION := 0.6.0-dev",make)
        self.assertIn("rm -rf /tmp/turnwave-native",build)
        self.assertIn('rm -f "$NATIVE/TurnwaveNX2.nro"',build)
        self.assertIn("draw_text(pixels,stride,98,672",source)
        self.assertIn("draw_text(pixels,stride,833,521",source)
        self.assertIn("glass((64,632,1216,706)",bg)
        self.assertIn("glass((792,501,1181,568)",bg)
        self.assertNotIn("A / X  Apple ID login via Chromium",bg)
        self.assertNotIn("draw_text(pixels,stride,742,570",source)

if __name__=="__main__":
    unittest.main()
