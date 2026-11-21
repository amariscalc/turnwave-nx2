"""Regression checks for the diagnostic-only GX gmem instrumentation."""
from pathlib import Path
import importlib.util
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location(
    "instrument_gmem",ROOT/"tools"/"instrument_gmem.py")
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class GmemInstrumentationTests(unittest.TestCase):
    def test_patches_known_switch2_source_without_allocator_policy_changes(self):
        original="\n".join((m.LOADER_OLD,m.OTHER_OLD,m.UNKNOWN_OLD,m.SVC_OLD))
        updated=m.patch_source(original)
        for marker in ("NX2_CDM_DIAG_V1","origin=loader","origin=other",
                       "origin=unknown","veneer=no-image","veneer=exhausted"):
            self.assertIn(marker,updated)
        for old in ("find_free_aligned(size / GX_PAGE, align, GX_ARENA_IMAGES)",
                    "find_free_aligned(size / GX_PAGE, align, GX_ARENA_DATA)",
                    "find_free_aligned(size / GX_PAGE, ialign, GX_ARENA_IMAGES)"):
            self.assertIn(old,updated)
        self.assertEqual(m.patch_source(updated),updated)

    def test_refuses_unrecognized_source_and_partial_patch(self):
        with self.assertRaisesRegex(ValueError,"SVC anchor"):
            m.patch_source("int main(void) { return 0; }")
        with self.assertRaisesRegex(ValueError,"Partially instrumented"):
            m.patch_source("/* NX2_CDM_DIAG_V1 */")

if __name__=="__main__":
    unittest.main()
