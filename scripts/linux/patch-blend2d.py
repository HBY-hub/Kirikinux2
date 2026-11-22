#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Fix the blend2d header for glibc builds.  Idempotent; run by CMakeLists.txt
before add_subdirectory(blend2d).

blend2d's api-build_p.h defines _LARGEFILE64_SOURCE itself, but only after the
translation unit's libc headers may already have been read, so `struct stat64`
is undefined and src/blend2d/filesystem.cpp fails on glibc:

    struct BL_FILE64_API(stat) s;   ->  error: incomplete type

CMakeLists.txt passes -D_LARGEFILE64_SOURCE=1 on the command line for blend2d.
That makes the guard in the header skip its body, which used to be the only
place defining BL_FILE64_API.  Keep the macro definition outside the guard so
both cases work.

    patch-blend2d.py <blend2d root>
"""
import pathlib
import sys

MARK = "[kirikinux2]"

OLD = """#if !defined(_WIN32) && !defined(_LARGEFILE64_SOURCE)
  #define _LARGEFILE64_SOURCE 1
"""

NEW = """#if !defined(_WIN32) // """ + MARK + """ BL_FILE64_API must also be defined when
  // the build system already passes -D_LARGEFILE64_SOURCE on the command line
  // (glibc only declares struct stat64 if the macro is set before <sys/stat.h>).
  #if !defined(_LARGEFILE64_SOURCE)
    #define _LARGEFILE64_SOURCE 1
  #endif
"""


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    path = pathlib.Path(sys.argv[1]) / "src/blend2d/api-build_p.h"
    if not path.exists():
        print(f"patch-blend2d: missing {path}")
        return 1
    text = path.read_text(encoding="utf-8", errors="surrogateescape")
    if MARK in text:
        return 0
    if OLD not in text:
        print("patch-blend2d: pattern not found (unexpected blend2d revision)")
        return 1
    path.write_text(text.replace(OLD, NEW, 1), encoding="utf-8",
                    errors="surrogateescape")
    print("patch-blend2d: patched src/blend2d/api-build_p.h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
