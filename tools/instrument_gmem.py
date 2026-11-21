#!/usr/bin/env python3
"""Reversible, diagnostic-only patch for gx/gmem.c on Switch 2.

No allocator policy changes. Copies original gmem.c to
gmem.c.before-turnwave-cdm-diag first. Run with --check for a dry run.
Does not touch profiles, CDMs or SD cards.
"""
from __future__ import annotations
import argparse
import re
import textwrap
from pathlib import Path
import shutil
import sys

MARKER = "NX2_CDM_DIAG_V1"

SVC_OLD = '''                if (!img || img->pool_next + VENEER_SIZE > img->pool_end) {
                        host_log("gx: no veneer left for the svc at %#lx\\n", (unsigned long)site);
                        ret = -1;
                        break;
                }'''
SVC_NEW = '''                /* NX2_CDM_DIAG_V1: report absent pools separately from exhausted pools. */
                if (!img) {
                        host_log("gx: NX2_CDM_DIAG veneer=no-image site=%#lx map=%#lx len=%#lx\\n",
                                 (unsigned long)site, (unsigned long)addr,
                                 (unsigned long)len);
                        ret = -1;
                        break;
                }
                if (img->pool_next + VENEER_SIZE > img->pool_end) {
                        host_log("gx: NX2_CDM_DIAG veneer=exhausted site=%#lx map=%#lx len=%#lx "
                                 "pool=%#lx used=%lu cap=%lu\\n",
                                 (unsigned long)site, (unsigned long)addr,
                                 (unsigned long)len, (unsigned long)img->pool,
                                 (unsigned long)(img->pool_next - img->pool),
                                 (unsigned long)(img->pool_end - img->pool));
                        ret = -1;
                        break;
                }'''
LOADER_OLD = '''        if (origin == ORIGIN_LOADER) {
                addr = find_free_aligned(size / GX_PAGE, align, GX_ARENA_IMAGES);'''
LOADER_NEW = '''        if (origin == ORIGIN_LOADER) {
                addr = find_free_aligned(size / GX_PAGE, align, GX_ARENA_IMAGES);
                /* NX2_CDM_DIAG_V1: observe the CDM-sized reservation even on success. */
                if (size >= (4UL << 20) && size <= (32UL << 20))
                        host_log("gx: NX2_CDM_DIAG origin=loader size=%lu align=%lu site=%#lx addr=%#lx\\n",
                                 (unsigned long)size, (unsigned long)align,
                                 (unsigned long)mmap_site, (unsigned long)addr);'''
OTHER_OLD = '''        if (origin == ORIGIN_OTHER)
                return find_free_aligned(size / GX_PAGE, align, GX_ARENA_DATA);'''
OTHER_NEW = '''        if (origin == ORIGIN_OTHER) {
                addr = find_free_aligned(size / GX_PAGE, align, GX_ARENA_DATA);
                if (none && size >= (4UL << 20) && size <= (32UL << 20))
                        host_log("gx: NX2_CDM_DIAG origin=other size=%lu align=%lu site=%#lx addr=%#lx\\n",
                                 (unsigned long)size, (unsigned long)align,
                                 (unsigned long)mmap_site, (unsigned long)addr);
                return addr;
        }'''
UNKNOWN_OLD = '''                addr = find_free_aligned(size / GX_PAGE, ialign, GX_ARENA_IMAGES);
                if (size >= (64UL << 20))'''
UNKNOWN_NEW = '''                addr = find_free_aligned(size / GX_PAGE, ialign, GX_ARENA_IMAGES);
                if (size >= (4UL << 20) && size <= (32UL << 20))
                        host_log("gx: NX2_CDM_DIAG origin=unknown size=%lu align=%lu site=%#lx addr=%#lx\\n",
                                 (unsigned long)size, (unsigned long)ialign,
                                 (unsigned long)mmap_site, (unsigned long)addr);
                if (size >= (64UL << 20))'''

def _pattern_for(old: str) -> re.Pattern[str]:
    """Match the exact C statements, tolerating tabs/spaces/CRLF indentation.

    This deliberately does NOT match different statements or changed logic.
    """
    lines = old.splitlines()
    def words(line: str) -> str:
        # Internal formatting spaces can differ, but tokens must be identical.
        return re.escape(line.strip()).replace(r"\ ", r"[ \t]+")
    pattern = r"(?m)^(?P<indent>[ \t]*)" + words(lines[0])
    for line in lines[1:]:
        pattern += r"[ \t]*\r?\n[ \t]*" + words(line)
    pattern += r"[ \t]*(?=\r?$)"
    return re.compile(pattern)

def patch_source(src: str) -> str:
    if MARKER in src:
        if all(needle in src for needle in (
            "veneer=no-image", "veneer=exhausted",
            "origin=loader", "origin=other", "origin=unknown"
        )):
            return src
        raise ValueError("Partially instrumented gmem.c: no changes applied")
    changes = ((SVC_OLD, SVC_NEW, "SVC"),
               (LOADER_OLD, LOADER_NEW, "loader"),
               (OTHER_OLD, OTHER_NEW, "other"),
               (UNKNOWN_OLD, UNKNOWN_NEW, "unknown"))
    # Validate all four anchors BEFORE editing anything on disk.
    patterns = []
    for old, new, name in changes:
        pattern = _pattern_for(old)
        found = list(pattern.finditer(src))
        if len(found) != 1:
            raise ValueError(
                f"Expected one {name} C block (whitespace-tolerant), "
                f"found {len(found)}; source unchanged"
            )
        patterns.append((pattern, new))

    for pattern, new in patterns:
        normalized = textwrap.dedent(new).strip("\n")
        def insert(match: re.Match[str]) -> str:
            indent = match.group("indent")
            return "\n".join(indent + line for line in normalized.splitlines())
        src, count = pattern.subn(insert, src, count=1)
        if count != 1:
            raise ValueError("Unexpected patch anchor disappeared; source unchanged")
    return src

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("chromium_repo",type=Path)
    ap.add_argument("--check",action="store_true",help="Check compatibility; never write")
    args=ap.parse_args()
    target=args.chromium_repo.expanduser().resolve()/"gx"/"gmem.c"
    if not target.is_file():
        print(f"ERROR: missing {target}",file=sys.stderr)
        return 1
    original=target.read_text(encoding="utf-8")
    try:
        updated=patch_source(original)
    except ValueError as e:
        print(f"ERROR: {e}",file=sys.stderr)
        return 1
    if updated==original:
        print("ALREADY INSTRUMENTED: no changes needed")
        return 0
    if args.check:
        print("CHECK OK: expected GX anchors found; no changes written")
        return 0
    backup=target.with_name("gmem.c.before-turnwave-cdm-diag")
    if not backup.exists():
        shutil.copy2(target,backup)
    target.write_text(updated,encoding="utf-8")
    print("PATCHED:",target)
    print("BACKUP:",backup)
    print("No allocation algorithm, profile or Widevine CDM has been changed")
    return 0

if __name__=="__main__":
    sys.exit(main())
