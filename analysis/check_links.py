#!/usr/bin/env python3
"""Cross-check: every relative Markdown link in README and docs/ points to a file
that exists.

Two things are checked:

1. **The target exists.** If a knowledge-base node name slips into the repo, the
   link points nowhere, and no mirroring repairs that — it only rewrites it once
   more. On 2026-11-17 seven such links had appeared.
2. **The label is derived from the file name**, i.e. `[04-hardware]` or
   `[docs/04-hardware.md]` or `[05-test-procedure]` — but not `[Safety]`.
   Mirroring rewrote BOTH, label and target. A link with a knowledge-base label
   and a repo target therefore points to the right file and is not caught by
   check 1 — on 2026-11-18 exactly that happened. It only shows when someone
   compares the mirror character by character.

   The comparison is case-insensitive, without `.md` and with umlauts resolved.
   That allows both existing spellings: README uses the full path as the label,
   and older docs wrote a file name with an umlaut where the file itself is
   spelled with the transliteration.

Background: the documentation used to be mirrored into the knowledge base, and the
links were rewritten to its node names (`04-hardware.md` → `23_hardware.md`). The
rewrite was one-way. The mirror was removed on 2026-11-22; the check stays as a guard.

Run from the project root:
    python3 analysis/check_links.py
"""

import pathlib
import re
import sys

# Node names of the knowledge base. If they show up in the repo, something got mixed up.
KB_NODE = re.compile(r"^\d\d_[a-z_]+\.md$")

UMLAUTS = (("ä", "ae"), ("ö", "oe"), ("ü", "ue"), ("ß", "ss"))


def norm(t):
    """Make label and target comparable: lower case, without .md, without umlauts."""
    t = t.lower()
    if t.endswith(".md"):
        t = t[:-3]
    for a, b in UMLAUTS:
        t = t.replace(a, b)
    return t


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    files = [root / "README.md"] + sorted((root / "docs").glob("*.md"))
    errors = []
    checked = 0

    mislabelled = []

    for f in files:
        if not f.exists():
            continue
        text = f.read_text()
        base = f.parent
        for m in re.finditer(r"\[([^\]]*)\]\(([^)#]+\.md)(#[^)]*)?\)", text):
            label, target = m.group(1), m.group(2)
            if target.startswith(("http://", "https://", "/")):
                continue
            checked += 1
            line = text[: m.start()].count("\n") + 1
            if not (base / target).exists():
                reason = ("knowledge-base node name in the repo"
                         if KB_NODE.match(target) else "file missing")
                errors.append(f"  {f.relative_to(root)}:{line}  {target}  ({reason})")
                continue
            # Check 2: the label must come from the file name.
            if norm(label) != norm(target):
                mislabelled.append(f"  {f.relative_to(root)}:{line}  "
                              f"[{label}]({target})  →  [{target[:-3]}]({target})")

    print(f"Relative links checked: {checked}")
    if errors:
        print("\nTARGETS DO NOT EXIST:")
        print("\n".join(errors))
    if mislabelled:
        print("\nLABEL DOES NOT MATCH TARGET (knowledge-base form in the repo):")
        print("\n".join(mislabelled))
    if errors or mislabelled:
        return 1
    print("All links point to existing files, all labels match.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
