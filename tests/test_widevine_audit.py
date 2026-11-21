"""Read-only Widevine scanner contract tests."""
import contextlib
import importlib.util
from io import StringIO
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("audit_widevine",ROOT/"tools"/"audit_widevine.py")
audit=importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)

class WidevineAuditTests(unittest.TestCase):
    def make_image(self,folder):
        image=Path(folder)/"profile.ext4"
        with image.open("wb") as f:
            f.truncate(2*1024*1024)
        with image.open("r+b") as f:
            f.seek(1024+56)
            f.write(b"\x53\xef")
        return image

    def test_detects_cdm_without_extracting_private_content(self):
        with tempfile.TemporaryDirectory() as temp:
            image=self.make_image(temp)
            listing=(
                "d/d 10:\tprofile/WidevineCdm\n"
                "r/r 20:\tprofile/WidevineCdm/4.10.3112.0/manifest.json\n"
                "r/r 21:\tprofile/WidevineCdm/4.10.3112.0/_platform_specific/linux_arm64/libwidevinecdm.so\n"
                "r/r 22:\tprofile/Cookies\n"
            )
            output=StringIO()
            with patch.object(audit.shutil,"which",return_value="/usr/bin/fls"),\
                 patch.object(audit.subprocess,"run",return_value=SimpleNamespace(returncode=0,stdout=listing)),\
                 contextlib.redirect_stdout(output):
                self.assertEqual(audit.inspect(image),0)
            text=output.getvalue()
            self.assertIn("libwidevinecdm.so",text)
            self.assertNotIn("Cookies",text)

    def test_rejects_invalid_image_without_running_fls(self):
        with tempfile.TemporaryDirectory() as temp:
            image=Path(temp)/"not-ext4.ext4"
            image.write_bytes(b"0"*(1024*1024+4096))
            with patch.object(audit.subprocess,"run") as run:
                self.assertEqual(audit.inspect(image),1)
                run.assert_not_called()

if __name__=="__main__":
    unittest.main()
