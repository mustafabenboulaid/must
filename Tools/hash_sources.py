#!/usr/bin/env python3
"""File hashes for CHANGELOG.md (brief §8.6: one writer per file, hashes per step).

  python Tools/hash_sources.py            print a markdown table of short SHA-256 hashes
  python Tools/hash_sources.py --check    compare files with the newest table in CHANGELOG.md
"""
import hashlib
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TRACKED = ["Plugins/GhostsOfNovember", "Tools", "build.bat", "run_tests.bat"]
SKIP_PARTS = {"Binaries", "Intermediate", "__pycache__"}


def tracked_files():
    for entry in TRACKED:
        path = ROOT / entry
        files = [path] if path.is_file() else sorted(p for p in path.rglob("*") if p.is_file())
        for file in files:
            if not SKIP_PARTS.intersection(file.relative_to(ROOT).parts):
                yield file


def short_hash(path):
    # Normalise line endings so Windows checkouts (CRLF) hash the same as Git's content.
    data = path.read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(data).hexdigest()[:12]


def table():
    rows = ["| File | SHA-256 (12) |", "|---|---|"]
    rows += [f"| `{f.relative_to(ROOT).as_posix()}` | `{short_hash(f)}` |" for f in tracked_files()]
    return "\n".join(rows)


def check():
    text = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")
    recorded = dict(re.findall(r"^\| `([^`]+)` \| `([0-9a-f]{12})` \|$", text.split("\n## ", 2)[1] if "\n## " in text else text, re.M))
    current = {f.relative_to(ROOT).as_posix(): short_hash(f) for f in tracked_files()}
    changed = sorted(k for k in current if recorded.get(k) != current[k])
    removed = sorted(k for k in recorded if k not in current)
    for name in changed:
        print(f"changed: {name}" if name in recorded else f"new:     {name}")
    for name in removed:
        print(f"removed: {name}")
    print(f"{len(changed) + len(removed)} difference(s) against the newest CHANGELOG entry")
    return 1 if changed or removed else 0


if __name__ == "__main__":
    sys.exit(check() if "--check" in sys.argv else print(table()))
