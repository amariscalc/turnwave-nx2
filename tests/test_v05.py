import importlib.util
import unittest
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'chromium-bridge'))
from install import patch_cdp,patch_host,js_header
from test_native_bridge import CDP_SAMPLE,HOST_SAMPLE

class UpgradeTests(unittest.TestCase):
    def test_fresh_and_upgrade_idempotent(self):
        for source,fn in ((CDP_SAMPLE,patch_cdp),(HOST_SAMPLE,patch_host)):
            upgraded=fn(source)
            self.assertEqual(fn(upgraded),upgraded)
        self.assertIn('tw_native_inject_play_target',patch_cdp(CDP_SAMPLE))
        self.assertIn('tw_play_mode',patch_host(HOST_SAMPLE))
        self.assertIn('!tw_play_mode && tw_sync_ready()',patch_host(HOST_SAMPLE))

    def test_v04_to_v05(self):
        from install_v04 import patch_cdp as old_cdp,patch_host as old_host
        cdp=patch_cdp(old_cdp(CDP_SAMPLE))
        host=patch_host(old_host(HOST_SAMPLE))
        self.assertEqual(cdp,patch_cdp(CDP_SAMPLE))
        self.assertEqual(host,patch_host(HOST_SAMPLE))

    def test_protocol_and_no_private_tokens(self):
        js=(ROOT/'chromium-bridge/sync.js').read_text()
        cfrag=(ROOT/'chromium-bridge/gx_music_bridge.cfrag').read_text()
        native=(ROOT/'native/source/main.c').read_text()
        self.assertIn('BEGIN|2',js)
        self.assertIn('catalogId',js)
        self.assertIn('setQueue({song:id})',js)
        self.assertIn('PLAY|REQUESTED|',js)
        self.assertIn('TURNWAVE_LIBRARY_V2',cfrag)
        self.assertIn('TURNWAVE_PLAY_V1',native)
        self.assertIn('No catalog ID',native)
        for s in (js,cfrag):
            self.assertNotIn('musicUserToken',s)
            self.assertNotIn('developerToken',s)

if __name__=='__main__':unittest.main()
