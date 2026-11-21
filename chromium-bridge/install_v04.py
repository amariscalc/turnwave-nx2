#!/usr/bin/env python3
"""Patch the user's existing Chromium NX2 GX tree for Turnwave native login.

Only source files are patched. Chromium EXT4 images and profile are untouched.
The installer removes previous Turnwave *UI* injection call sites so Chromium
can show Apple ID during login, and adds metadata-only MusicKit synchronization.
"""
from pathlib import Path
import argparse
import json
import re
import sys
import shutil

HERE = Path(__file__).resolve().parent
MARK = '/* TURNWAVE_NATIVE_MUSICKIT_BRIDGE_V1 */'
SD_NATIVE = 'sdmc:/switch/TurnwaveNX2/TurnwaveNX2.nro'
HOST_MARK = '/* TURNWAVE_NATIVE_HOST_BRIDGE_V1 */'


def expect_once(source, needle):
    count = source.count(needle)
    if count != 1:
        raise ValueError(f'Expected one occurrence of {needle!r}, got {count}. No changes written.')


def js_header(source):
    fragments = [json.dumps(source[i:i+160], ensure_ascii=False).replace('?', r'\?')
                 for i in range(0, len(source), 160)]
    return ('/* Turnwave NX2 MusicKit metadata bridge; generated, do not edit. */\n'
            '#ifndef TW_NATIVE_SYNC_EMBED_H\n#define TW_NATIVE_SYNC_EMBED_H\n'
            'static const char tw_native_sync_script[] =\n' + '\n'.join('    '+s for s in fragments)
            + ';\n#endif\n')


def patch_cdp(src):
    # Remove the previous browser-UI overlay. The login page must remain visible.
    src = re.sub(r'(?m)^[ \t]*turnwave_nx2_inject\((?:sid|id)\);[ \t]*$',
                 '    /* Native mode: old browser overlay disabled. */', src)
    if MARK in src:
        return src
    expect_once(src, '#include "cdp.h"')
    for anchor in ('static void inject_again(', 'static void handle_window_message(',
                   'static void json_escape(', 'static void send_text('):
        expect_once(src, anchor)
    src = src.replace('#include "cdp.h"', '#include "cdp.h"\n#include "tw_native_sync_embed.h"\n#include <sys/stat.h>', 1)

    # Large dynamic DevTools messages: existing send_command() uses a 5 KB buffer.
    glue = r'''/* TURNWAVE_NATIVE_MUSICKIT_BRIDGE_V1 */
static volatile int tw_native_sync_complete;
static FILE *tw_native_sync_file;
static unsigned tw_native_sync_items;
int tw_sync_ready(void) { return __atomic_load_n(&tw_native_sync_complete, __ATOMIC_ACQUIRE); }
static int tw_unhex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
static void tw_decode(char *dst, size_t cap, const char *src)
{
    size_t n = 0;
    while (*src && n + 1 < cap) {
        unsigned char c = (unsigned char)*src++;
        if (c == '%' && src[0] && src[1]) {
            int a = tw_unhex(src[0]), b = tw_unhex(src[1]);
            if (a >= 0 && b >= 0) { c = (unsigned char)(a * 16 + b); src += 2; }
        }
        if (c < 32 || c == 127 || c == '\t' || c == '\r' || c == '\n') c = ' ';
        dst[n++] = (char)c;
    }
    dst[n] = 0;
}
static void tw_sync_message(const char *message)
{
    if (!strcmp(message, "BEGIN|1")) {
        if (tw_native_sync_file) fclose(tw_native_sync_file);
        mkdir("sdmc:/switch/TurnwaveNX2", 0777);
        tw_native_sync_file = fopen("sdmc:/switch/TurnwaveNX2/library.tsv.part", "w");
        tw_native_sync_items = 0;
        if (tw_native_sync_file) fputs("TURNWAVE_LIBRARY_V1\n", tw_native_sync_file);
        return;
    }
    if (!strcmp(message, "ABORT")) {
        if (tw_native_sync_file) fclose(tw_native_sync_file);
        tw_native_sync_file = NULL;
        remove("sdmc:/switch/TurnwaveNX2/library.tsv.part");
        return;
    }
    if (!strcmp(message, "END")) {
        if (!tw_native_sync_file) return;
        int ok = fflush(tw_native_sync_file) == 0;
        ok &= fclose(tw_native_sync_file) == 0;
        tw_native_sync_file = NULL;
        if (ok) {
            remove("sdmc:/switch/TurnwaveNX2/library.tsv");
            ok = rename("sdmc:/switch/TurnwaveNX2/library.tsv.part",
                        "sdmc:/switch/TurnwaveNX2/library.tsv") == 0;
        }
        if (ok) {
            host_log("gx: turnwave: exported %u library entries (metadata only)\n", tw_native_sync_items);
            __atomic_store_n(&tw_native_sync_complete, 1, __ATOMIC_RELEASE);
        } else host_log("gx: turnwave: could not save library metadata\n");
        return;
    }
    if (!tw_native_sync_file || tw_native_sync_items >= 180 || strncmp(message, "ITEM|", 5)) return;
    char kind = message[5];
    if (!strchr("PASRN", kind) || message[6] != '|') return;
    const char *sep = strchr(message + 7, '|');
    if (!sep) return;
    char name[168], detail[168], encoded[600];
    size_t n = (size_t)(sep - (message + 7));
    if (n >= sizeof(encoded)) return;
    memcpy(encoded, message + 7, n);
    encoded[n] = 0;
    tw_decode(name, sizeof(name), encoded);
    tw_decode(detail, sizeof(detail), sep + 1);
    if (!name[0]) return;
    if (fprintf(tw_native_sync_file, "%c\t%s\t%s\n", kind, name, detail) > 0)
        tw_native_sync_items++;
}
static void tw_native_send(const char *sid, const char *method, const char *field)
{
    size_t len = strlen(tw_native_sync_script);
    if (len > 128000) return;
    size_t cap = len * 6 + 1024;
    char *escaped = malloc(cap), *msg = malloc(cap + 512);
    if (!escaped || !msg) { free(escaped); free(msg); return; }
    json_escape(escaped, cap, tw_native_sync_script);
    int n = snprintf(msg, cap + 512,
        "{\"id\":%d,\"sessionId\":\"%s\",\"method\":\"%s\",\"params\":{\"%s\":\"%s\"}}",
        next_id++, sid, method, field, escaped);
    if (n > 0 && (size_t)n < cap + 512) send_text(msg, (size_t)n);
    free(msg); free(escaped);
}
static void tw_native_inject(const char *sid)
{
    send_command(sid, "Runtime.addBinding", "{\"name\":\"gxTurnwaveSync\"}");
    tw_native_send(sid, "Page.addScriptToEvaluateOnNewDocument", "source");
    tw_native_send(sid, "Runtime.evaluate", "expression");
}

'''
    src = src.replace('static void inject_again(', glue + 'static void inject_again(', 1)
    a = src.index('static void inject_again(')
    b = src.index('static void handle_window_message(', a)
    chunk = src[a:b]
    old = 'send_command(sid, "Runtime.evaluate", params);'
    expect_once(chunk, old)
    chunk = chunk.replace(old, old + '\n\ttw_native_inject(sid);', 1)
    src = src[:a] + chunk + src[b:]
    needle = '/* The first page has the keyboard from the start'
    expect_once(src, needle)
    src = src.replace(needle, 'tw_native_inject(id);\n\t\t' + needle, 1)
    # Capture encoded metadata from the same-origin script. Never log credentials.
    # Preserve the existing gxEdit handler without depending on tabs/spaces.
    pattern = r'\}\s*else if \(strstr\(message, [^\n]*Runtime\.bindingCalled[^\n]*\n\s*strstr\(message, [^\n]*gxEdit[^\n]*\) \{'
    match = re.search(pattern, src)
    if not match:
        raise ValueError('GX binding handler changed; refusing to patch')
    replacement = """} else if (strstr(message, "\\\"method\\\":\\\"Runtime.bindingCalled\\\"") &&
               strstr(message, "\\\"name\\\":\\\"gxTurnwaveSync\\\"")) {
        char payload[1400];
        if (string_value(message, "payload", payload, sizeof(payload)) > 0)
            tw_sync_message(payload);
    """
    src = src[:match.start()] + replacement + src[match.start():]
    return src


def patch_host(src):
    if HOST_MARK in src: return src
    marker = 'static int browser_mode;'
    expect_once(src, marker)
    src = src.replace(marker,marker + '''
/* TURNWAVE_NATIVE_HOST_BRIDGE_V1 */
/* Native Turnwave starts this NRO with --browser --turnwave-login. */
static int tw_login_mode;
extern int tw_sync_ready(void);''',1)
    marker = 'browser_mode = args && strstr(args, "--browser");'
    expect_once(src,marker)
    src=src.replace(marker,marker+'\n\ttw_login_mode = args && strstr(args, "--turnwave-login");',1)
    marker='\tread_commands();'
    expect_once(src,marker)
    src=src.replace(marker,marker+'''
    if (tw_login_mode) {
        /* Use GX's complete existing browser runtime and profile, but force the
         * Apple Music login URL; no Chromium launcher or generic homepage. */
        const char *default_cmd = gx_default_command();
        snprintf(commands[0],sizeof(commands[0]),"%s",default_cmd);
        char *url = strrchr(commands[0],' ');
        if (url && (!strncmp(url+1,"https://",8) || !strncmp(url+1,"--app=",6))) *url=0;
        strncat(commands[0]," https://music.apple.com/es/",
                sizeof(commands[0])-strlen(commands[0])-1);
        command_count=1;
    }
''',1)
    marker='\t\tif (__atomic_load_n(&quit_wanted, __ATOMIC_SEQ_CST))'
    expect_once(src,marker)
    src=src.replace(marker,'''		/* MusicKit supplied library data: close Chromium automatically. */
        if (tw_login_mode && tw_sync_ready()) break;
''' + marker,1)
    # When login mode ends, let hbloader chainload the native Turnwave NRO.
    # Crucial: GX's normal leave() uses svcExitProcess(), which skips normal
    # main() unwinding. The next load is requested only after Chromium closes.
    anchor='\tleave();\n\treturn 0;\n}'
    if src.count(anchor)<1:raise ValueError('GX browser tail changed')
    # Tail is the last occurrence; leave() also used elsewhere.
    at=src.rfind(anchor)
    src=src[:at]+'''    if (tw_login_mode) {
        Result rc=envSetNextLoad("sdmc:/switch/TurnwaveNX2/TurnwaveNX2.nro",
                                 "sdmc:/switch/TurnwaveNX2/TurnwaveNX2.nro");
        if (R_SUCCEEDED(rc)) {
            return 0;  /* do not use GX's svcExitProcess() path */
        }
    }
''' + src[at:]
    return src


def install(repo, check_only=False):
    repo=repo.expanduser().resolve()
    files={repo/'gx/cdp.c':patch_cdp,repo/'gx/host_nx.c':patch_host}
    old={};new={}
    for path,fn in files.items():
        if not path.is_file():raise ValueError(f'Missing Chromium source: {path}')
        old[path]=path.read_text(encoding='utf-8')
        new[path]=fn(old[path])
    js=(HERE/'sync.js').read_text(encoding='utf-8')
    if 'mk.api.music' not in js or 'gxTurnwaveSync' not in js:raise ValueError('Invalid sync script')
    if check_only:
        print('CHECK OK: Chromium MusicKit bridge can be patched.')
        for p in files:print(p.relative_to(repo),'needs patch' if old[p]!=new[p] else 'already patched')
        return
    for p in files:
        if old[p] != new[p]:
            backup=p.with_name(p.name+'.before-turnwave-native')
            if not backup.exists():shutil.copy2(p,backup)
            p.write_text(new[p],encoding='utf-8')
    (repo/'gx/tw_native_sync_embed.h').write_text(js_header(js),encoding='utf-8')
    print('MusicKit metadata sync installed. No EXT4 images or cookies were modified.')
    print('Build Chromium NX2 using the existing working Docker command.')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('chromium_repo',type=Path)
    ap.add_argument('--check',action='store_true')
    a=ap.parse_args()
    try:install(a.chromium_repo,a.check)
    except (ValueError,OSError) as e:
        print('ERROR:',e,file=sys.stderr);sys.exit(1)

if __name__=='__main__':main()
