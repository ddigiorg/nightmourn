#!/usr/bin/env python3
# ==============================================================================
# tools/format_json.py
#
# Formats Nightmourn's config JSON (data/*/configs/**/*.json) in one house
# style, so every file reads the same and diffs stay small:
#
#   - 4-space indent, keys in the order they were written
#   - every object on its own lines, one key per line
#   - short arrays of numbers, booleans or strings on one line, and so are
#     arrays of those arrays: "pivot": [32, 96],
#     "collision_box": [[-0.25, -0.25], [0.25, 0.25]]
#   - a newline at the end of the file
#
# Usage:
#   python3 tools/format_json.py            rewrite every config in data/
#   python3 tools/format_json.py --check    list files that aren't formatted,
#                                           exit 1 if there are any
#   python3 tools/format_json.py FILE ...   just these files
# ==============================================================================

import json
import sys
from pathlib import Path

INDENT     = "    "
INLINE_MAX = 72  # longest array written on one line, in characters


def scalar(value):
    return not isinstance(value, (dict, list))


def inline(value):
    """`value` on one line if it is a short array of scalars (or of such arrays)."""
    if not isinstance(value, list):
        return None
    if not all(scalar(v) or (isinstance(v, list) and all(scalar(w) for w in v)) for v in value):
        return None
    text = json.dumps(value, ensure_ascii=False, separators=(", ", ": "))
    return text if len(text) <= INLINE_MAX else None


def dump(value, depth=0):
    pad, inner = INDENT * depth, INDENT * (depth + 1)

    if scalar(value):
        return json.dumps(value, ensure_ascii=False)

    one_line = inline(value)
    if one_line is not None:
        return one_line

    if isinstance(value, list):
        if not value:
            return "[]"
        items = [inner + dump(v, depth + 1) for v in value]
        return "[\n" + ",\n".join(items) + "\n" + pad + "]"

    if not value:
        return "{}"
    items = [inner + json.dumps(k, ensure_ascii=False) + ": " + dump(v, depth + 1) for k, v in value.items()]
    return "{\n" + ",\n".join(items) + "\n" + pad + "}"


def format_file(path):
    """Formatted text of `path`, or raises naming the file on bad JSON."""
    text = path.read_text(encoding="utf-8")
    try:
        doc = json.loads(text)
    except json.JSONDecodeError as e:
        raise SystemExit(f"{path}: {e}")
    return text, dump(doc) + "\n"


def main(argv):
    check = "--check" in argv
    files = [Path(a) for a in argv if a != "--check"]
    if not files:
        root  = Path(__file__).resolve().parent.parent / "data"
        files = sorted(root.glob("*/configs/**/*.json"))

    unformatted = []
    for path in files:
        old, new = format_file(path)
        if old == new:
            continue
        unformatted.append(path)
        if not check:
            path.write_text(new, encoding="utf-8")

    for path in unformatted:
        print(("unformatted: " if check else "formatted: ") + str(path))
    return 1 if check and unformatted else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
