#!/usr/bin/env python3
"""Install or upgrade Turnwave native MusicKit V2 bridge in GX/Chromium NX2.

Works with an unpatched chromium-nx tree or a tree patched by Turnwave v0.4.
Never changes EXT4 images or profile contents. Re-running is idempotent.
"""
from pathlib import Path
import argparse
import shutil
import sys
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
from install_v04 import patch_cdp as patch_cdp_v04, patch_host as patch_host_v04, js_header
V1='/* TURNWAVE_NATIVE_MUSICKIT_BRIDGE_V1 */'
HOST='/* TURNWAVE_NATIVE_HOST_BRIDGE_V1 */'

def patch_cdp(source):
    src=patch_cdp_v04(source) if V1 not in source else source
    begin=src.index(V1)
    end=src.index('static void inject_again(',begin)
    glue=(HERE/'gx_music_bridge.cfrag').read_text(encoding='utf8')
    return src[:begin]+glue+src[end:]

def patch_host(source):
    src=patch_host_v04(source) if HOST not in source else source
    original='static int tw_login_mode;'
    replaced='static int tw_login_mode;\nstatic int tw_play_mode;'
    if replaced not in src:
        if src.count(original)!=1:raise ValueError('Host cannot locate tw_login_mode declaration')
        src=src.replace(original,replaced,1)
    original='tw_login_mode = args && strstr(args, "--turnwave-login");'
    replaced=('tw_play_mode = args && strstr(args, "--turnwave-play");\n\t'
              'tw_login_mode = args && (strstr(args, "--turnwave-login") || tw_play_mode);')
    if replaced not in src:
        if src.count(original)!=1:raise ValueError('Host cannot locate --turnwave-login switch')
        src=src.replace(original,replaced,1)
    original='tw_login_mode && tw_sync_ready()'
    replaced='tw_login_mode && !tw_play_mode && tw_sync_ready()'
    if replaced not in src:
        if src.count(original)!=1:raise ValueError('Host cannot locate sync completion handler')
        src=src.replace(original,replaced,1)
    return src

def install(repo,check=False):
    repo=repo.resolve()
    files={repo/'gx/cdp.c':patch_cdp,repo/'gx/host_nx.c':patch_host}
    old={};new={}
    for p,patch in files.items():
        if not p.is_file():raise ValueError(f'Missing source file: {p}')
        old[p]=p.read_text(encoding='utf8')
        new[p]=patch(old[p])
    js=(HERE/'sync.js').read_text(encoding='utf8')
    header=js_header(js)
    header_path=repo/'gx/tw_native_sync_embed.h'
    previous=header_path.read_text(encoding='utf8') if header_path.exists() else None
    if check:
        print('CHECK OK: native music bridge V2 / playback diagnostics')
        for p in files:print(str(p.relative_to(repo)), 'change needed' if old[p]!=new[p] else 'up to date')
        print(str(header_path.relative_to(repo)), 'change needed' if previous!=header else 'up to date')
        return
    for p in files:
        if old[p]!=new[p]:
            backup=p.with_name(p.name+'.before-turnwave-v05')
            if not backup.exists():shutil.copy2(p,backup)
            p.write_text(new[p],encoding='utf8')
    if previous!=header:header_path.write_text(header,encoding='utf8')
    print('Installed Turnwave V2 metadata IDs + MusicKit playback probe.')
    print('Chromium EXT4 images, saved credentials and profile were not modified.')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('chromium_repo',type=Path)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    try:install(args.chromium_repo,args.check)
    except (ValueError,OSError) as error:
        print('ERROR:',error,file=sys.stderr)
        sys.exit(1)

if __name__=='__main__':main()
