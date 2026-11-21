#!/usr/bin/env python3
"""Recover an OFFLINE copy of Chromium NX2 profile.ext4 without overwriting it.

The input must be copied off the console first, after Chromium has exited.
Only the separate repaired output may be uploaded back to the SD card.
"""
import argparse
from datetime import datetime
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

LOCKS = (
    "/profile/SingletonLock",
    "/profile/SingletonCookie",
    "/profile/SingletonSocket",
)

def command(*args):
    result = subprocess.run(args, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, check=False)
    print("$", " ".join(str(arg) for arg in args))
    if result.stdout:
        print(result.stdout[-12000:])
    return result.returncode

def ext4_image(path):
    if not path.is_file() or path.stat().st_size < 1024 * 1024:
        return False
    with path.open("rb") as stream:
        stream.seek(1080)
        return stream.read(2) == b"\x53\xef"

def exists_in_image(image, name):
    result = subprocess.run(["debugfs", "-R", "stat " + name, str(image)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, check=False)
    return "Inode:" in result.stdout and "File not found" not in result.stdout

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", type=Path,
                        help="local OFFLINE copy of profile.ext4 from the SD card")
    parser.add_argument("--output-dir", type=Path, default=Path.home() / "turnwave-profile-recovery")
    args = parser.parse_args()

    for tool in ("e2fsck", "debugfs"):
        if not shutil.which(tool):
            parser.error("Missing " + tool + "; install with: sudo apt install e2fsprogs")

    source = args.profile.expanduser().resolve()
    if not ext4_image(source):
        parser.error("Input is not a valid EXT filesystem image of at least 1 MiB")

    parent = args.output_dir.expanduser().resolve()
    parent.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix="run-" + datetime.now().strftime("%Y%m%d-%H%M%S") + "-",
                                   dir=parent))
    backup = folder / "profile.ext4.original"
    repaired = folder / "profile.ext4.repaired"
    shutil.copy2(source, backup)
    shutil.copy2(backup, repaired)
    print("Original preserved:", backup)
    print("Working copy:", repaired)
    print("Repairing only the copy; NEVER run this while Chromium is active.")

    # -y can discard damaged directory entries, but the original byte-for-byte
    # backup remains intact. Do not continue if fsck cannot reach consistency.
    result = command("e2fsck", "-f", "-y", str(repaired))
    if result not in (0, 1):
        print("ERROR: e2fsck could not finish; keep original and do not upload repaired image.", file=sys.stderr)
        return 1

    # Under GX, the image is mounted as /root; the browser uses /root/profile.
    # These are Chromium's ephemeral single-instance sentinels, NOT credentials.
    for name in LOCKS:
        if exists_in_image(repaired, name):
            result = command("debugfs", "-w", "-R", "rm " + name, str(repaired))
            if result or exists_in_image(repaired, name):
                print("ERROR: unable to remove stale lock " + name, file=sys.stderr)
                return 1

    result = command("e2fsck", "-f", "-n", str(repaired))
    if result:
        print("ERROR: repaired copy still fails read-only verification.", file=sys.stderr)
        return 1

    print("\nRECOVERY OK (offline fsck check passed).")
    print("Keep backup:", backup)
    print("Copy THIS image to SD switch/chromium-nx-diagnostico/profile.ext4:", repaired)
    print("This does not confirm Chromium will boot: test on the console.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
