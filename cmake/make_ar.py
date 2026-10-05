#!/usr/bin/env python3
"""Write a GNU ar archive (used for the webOS .ipk container).

Usage: make_ar.py OUTPUT MEMBER [MEMBER ...]
Members are stored by basename with GNU "/" name terminator, mode 644,
uid/gid 0, mtime 0 for reproducible builds. Stdlib only.
"""
import os
import sys


def ar_entry(name, data):
    name = name.encode() + b"/"
    header = b"%-16s%-12d%-6d%-6d%-8o%-10d`\n" % (
        name, 0, 0, 0, 0o100644, len(data),
    )
    assert len(header) == 60, len(header)
    pad = b"\n" if len(data) % 2 else b""
    return header + data + pad


def main():
    out, members = sys.argv[1], sys.argv[2:]
    with open(out, "wb") as f:
        f.write(b"!<arch>\n")
        for m in members:
            with open(m, "rb") as fh:
                f.write(ar_entry(os.path.basename(m), fh.read()))
    print(f"wrote {out} ({os.path.getsize(out)} bytes)")


if __name__ == "__main__":
    main()
