import unittest
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('bridge',ROOT/'chromium-bridge'/'install.py')
bridge=importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)

CDP_SAMPLE='''#include "cdp.h"
static int next_id;
static void send_text(const char *t, int n) {}
static void send_command(const char *sid, const char *method, const char *params) {}
static void json_escape(char *dst, unsigned n, const char *s) {}
static void inject_again(const char *message) {
    char sid[128],params[100];
    send_command(sid, "Runtime.evaluate", params);
    turnwave_nx2_inject(sid);
}
static void handle_window_message(const char *message) {
    if (0) {}
    else if (strstr(message, "\\"method\\":\\"Runtime.bindingCalled\\"") &&
\t\t   strstr(message, "\\"name\\":\\"gxEdit\\"")) {
        return;
    }
    /* The first page has the keyboard from the start */
    turnwave_nx2_inject(id);
}
'''
HOST_SAMPLE='''static int browser_mode;
void boot(const char *args) {
    browser_mode = args && strstr(args, "--browser");
\tread_commands();
}
void main_loop(void) {
\t\tif (__atomic_load_n(&quit_wanted, __ATOMIC_SEQ_CST)) break;
}
void final(void) {
\tleave();
\treturn 0;
}
'''

class BridgeTests(unittest.TestCase):
    def test_header_preserves_script(self):
        js=(ROOT/'chromium-bridge'/'sync.js').read_text(encoding='utf8')
        hdr=bridge.js_header(js)
        self.assertIn('tw_native_sync_script',hdr)
        self.assertIn('MusicKit',js)
        self.assertIn('mk.api.music',js)
        self.assertNotIn('musicUserToken',js)
        self.assertNotIn('developerToken',js)

    def test_host_patch_reentrant(self):
        patched=bridge.patch_host(HOST_SAMPLE)
        self.assertIn('tw_login_mode',patched)
        self.assertIn('tw_sync_ready()',patched)
        self.assertIn('envSetNextLoad("sdmc:/switch/TurnwaveNX2/TurnwaveNX2.nro"',patched)
        self.assertEqual(patched, bridge.patch_host(patched))

    def test_cdp_patch_reentrant(self):
        patched=bridge.patch_cdp(CDP_SAMPLE)
        self.assertIn('gxTurnwaveSync',patched)
        self.assertIn('tw_sync_message(payload)',patched)
        self.assertEqual(patched, bridge.patch_cdp(patched))
        self.assertNotIn('\n    turnwave_nx2_inject(sid);',patched)

    def test_native_resource_sizes(self):
        home=ROOT/'native/build/ui/home.rgba'
        self.assertEqual(home.stat().st_size,1280*720*4)
        for sz,cw,ch in ((16,27,30),(22,34,38),(32,50,52)):
            self.assertEqual((ROOT/f'native/build/ui/font{sz}.alpha').stat().st_size,224*cw*ch)
            self.assertEqual((ROOT/f'native/build/ui/font{sz}.widths').stat().st_size,224)

if __name__=='__main__':unittest.main()
