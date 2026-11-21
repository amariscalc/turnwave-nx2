#!/usr/bin/env python3
"""Read-only comparison of Chromium/GX EXT4 files for Widevine artifacts.

Usage:
  python3 tools/audit_widevine.py /mnt/c/NintendoSwitch/chromium-full.ext4
  python3 tools/audit_widevine.py /mnt/c/NintendoSwitch/old-profile.ext4 /mnt/c/NintendoSwitch/new-profile.ext4

Requires sleuthkit ('fls'). Reads filenames only, never extracts cookies,
tokens, passwords or files; does not mount or repair EXT4 images.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import sys

NAME_RE=re.compile(r'(widevine|libwidevinecdm|(?:^|[/\\])[^/\\]*cdm[^/\\]*)',re.I)
EXT4_MAGIC=b'\x53\xef'
SUPERBLOCK_MAGIC_OFFSET=1024+56

def inspect(path:Path)->int:
    print(f"\n=== {path.name} ===")
    if not path.is_file():
        print("ERROR: file not found")
        return 1
    size=path.stat().st_size
    print(f"Size: {size:,} bytes")
    if size<1024*1024:
        print("ERROR: image too small to be a usable EXT4 filesystem")
        return 1
    with path.open('rb') as f:
        f.seek(SUPERBLOCK_MAGIC_OFFSET)
        valid=(f.read(2)==EXT4_MAGIC)
    if not valid:
        print("ERROR: invalid EXT-family superblock magic")
        return 1
    print("EXT filesystem signature: valid")
    if not shutil.which('fls'):
        print("ERROR: fls missing; install with sudo apt install sleuthkit")
        return 1
    # 'fls' inspects the image read-only. No file contents or credentials.
    try:
        result=subprocess.run(['fls','-r','-p',str(path)],capture_output=True,text=True,
                              timeout=120,errors='replace',check=False)
    except subprocess.TimeoutExpired:
        print("ERROR: fls timed out after 120 seconds")
        return 1
    if result.returncode != 0:
        print(f"ERROR: fls returned {result.returncode}")
        print("Hint: verify the image is a complete, closed, unmounted copy.")
        return 1
    candidates=[]
    for line in result.stdout.splitlines():
        if '\t' not in line:
            continue
        name=line.split('\t',1)[1]
        if NAME_RE.search(name):
            candidates.append(name)
    candidates=list(dict.fromkeys(candidates))
    if candidates:
        print(f"Widevine/CDM-related filenames: {len(candidates)}")
        for name in candidates[:60]:
            print(" ",name[:230])
        if len(candidates)>60:
            print(f"  ... {len(candidates)-60} additional matching filenames omitted")
    else:
        print("No Widevine/CDM-related filenames found in this EXT4 image.")
    print("Note: a registered key system is not proof of successful CDM loading or decryption.")
    return 0

def main()->int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('images',type=Path,nargs='+',help="Offline copies of EXT4 images")
    args=parser.parse_args()
    errors=sum(inspect(p) for p in args.images)
    if len(args.images)>1:
        print("\nCompare results by image; do not merge or overwrite profiles.")
    return int(bool(errors))

if __name__=='__main__':
    sys.exit(main())
