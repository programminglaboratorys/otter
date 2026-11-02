"""Turns the Windows probe results into readable C++ test cases.

    python tests/make_cases.py <probe_results.json> [<probe_results.json> ...]

Writes tests/cases_probe.hpp. Only the calls the test shim supports are converted:
WritePrivateProfileStringA (set/delete) and GetPrivateProfileStringA (read),
with plain ASCII arguments, where every write succeeded. Everything else is
skipped and counted at the end. Identical cases (same file before, same calls) are kept once.
"""
import ast
import collections
import json
import os
import re
import string
import sys

MAX_BYTES = 800  # skip cases whose files are too big to read comfortably
SKIP_GROUPS = {"encoding", "null", "path", "perf", "lock", "cache", "struct", "external"}


def cpp_str(data: bytes) -> str:
    """C++ string literal, one source line per file line."""
    if not data:
        return '""'
    pieces = [p + b"\n" for p in data.split(b"\n")[:-1]]
    if not data.endswith(b"\n"):
        pieces.append(data.split(b"\n")[-1])
    out = []
    for piece in pieces:
        s, after_hex = "", False
        for c in piece:
            ch = chr(c)
            if ch == "\\":
                s += "\\\\"
            elif ch == '"':
                s += '\\"'
            elif ch == "\r":
                s += "\\r"
            elif ch == "\n":
                s += "\\n"
            elif ch == "\t":
                s += "\\t"
            elif 32 <= c < 127:
                # "\x1a" followed by "5" would read as one escape: split the literal
                s += ('" "' if after_hex and ch in string.hexdigits else "") + ch
            else:
                s += "\\x%02x" % c
                after_hex = True
                continue
            after_hex = False
        out.append('"' + s + '"')
    return "\n         ".join(out)


def ascii_args(args):
    """True if every argument is None or plain ASCII bytes."""
    return all(a is None or (isinstance(a, bytes) and all(c < 128 for c in a)) for a in args)


def parse_write(desc):
    """'WritePrivateProfileStringA(b"A", b"k", None, "x.ini")' -> C++ call, or None."""
    m = re.match(r"WritePrivateProfileStringA\((.*)\)$", desc)
    if not m:
        return None
    try:
        sec, key, val, _file = ast.literal_eval("(" + m.group(1) + ",)")
    except (ValueError, SyntaxError):
        return None
    if not ascii_args((sec, key, val)) or sec is None:
        return None  # non-ASCII (the library is UTF-8, later) or NULL section (no shim form)
    s = cpp_str(sec)
    if key is None:
        return "b.DeleteGroup(%s);" % s
    if val is None:
        return "b.DeleteItem(%s, %s);" % (s, cpp_str(key))
    return "b.SetString(%s, %s, %s);" % (s, cpp_str(key), cpp_str(val))


def parse_read(step):
    """A GetPrivateProfileStringA step -> a C++ check of what Windows returned, or None."""
    m = re.match(r"GetPrivateProfileStringA\((.*)\)$", step["desc"])
    if not m:
        return None
    try:
        sec, key, default, size, _file = ast.literal_eval("(" + m.group(1) + ",)")
        out = ast.literal_eval(step["out"]).encode("latin-1")
    except (ValueError, SyntaxError, KeyError, UnicodeEncodeError):
        return None
    if sec is None or key is None or not ascii_args((sec, key, default)):
        return None  # NULL section/key list names: the shim has no form for that
    if size < 256 or len(out) >= size - 1 or b"\0" in out:
        return None  # truncated by a small buffer: the shim reads with a big one
    return 'expect_eq(b.getString(%s, %s, %s), std::string(%s), "GetString");' % (
        cpp_str(sec), cpp_str(key), cpp_str(default or b""), cpp_str(out))


def convert(case, skipped):
    if case["group"] in SKIP_GROUPS:
        skipped["group not supported yet"] += 1
        return None
    calls, writes = [], 0
    for step in case["steps"]:
        if step["desc"].startswith("READBACK"):
            continue  # only informational
        if step["desc"].startswith("GetPrivateProfileStringA("):
            call = parse_read(step)
            if call:
                calls.append(call)
            continue  # a read we can't express is dropped: reads never change the file
        call = parse_write(step["desc"])
        if call is None or step.get("ret") != 1:
            skipped["uses a call/argument the shim can't do"] += 1
            return None
        calls.append(call)
        writes += 1
    if not calls:
        skipped["no usable result"] += 1
        return None
    before = bytes.fromhex(case["before"]["hex"]) if case["before"] else None
    after = bytes.fromhex(case["after"]["hex"]) if case["after"] else None
    if after is None and writes:
        skipped["no usable result"] += 1
        return None
    if (after and len(after) > MAX_BYTES) or (before and len(before) > MAX_BYTES):
        skipped["file too big to read"] += 1
        return None
    return before, calls, after


def slug(title):
    return re.sub(r"[^a-z0-9]+", "_", title.lower()).strip("_")[:45]


def main(paths):
    skipped, seen, blocks = collections.Counter(), set(), []
    for n, path in enumerate(paths, 1):
        for case in json.load(open(path, encoding="utf-8"))["results"]:
            got = convert(case, skipped)
            if got is None:
                continue
            before, calls, after = got
            key = (before, tuple(calls))
            if key in seen:
                skipped["duplicate"] += 1
                continue
            seen.add(key)
            name = "p%d_%s_%s" % (n, case["id"], slug(case["title"]))
            body = "\n".join("             " + c for c in calls)
            blocks.append(
                '        // %s: %s\n'
                '        {"%s",\n         %s,\n'
                '         [](Backend& b) {\n%s\n         },\n         %s},\n'
                % (case["id"], case["title"], name,
                   "std::nullopt" if before is None else cpp_str(before), body,
                   "std::nullopt" if after is None else cpp_str(after)))

    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cases_probe.hpp")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("// GENERATED by tests/make_cases.py from the Windows probe results. Do not edit.\n"
                "// Each case: file before, calls, exact file bytes Windows left after.\n"
                "#pragma once\n\n#include <vector>\n\n#include \"harness.hpp\"\n\n"
                "namespace cases {\n\nusing harness::Case;\nusing harness::expect_eq;\nusing shim::Backend;\n\n"
                "inline std::vector<Case> probe() {\n    return {\n")
        f.write("\n".join(blocks))
        f.write("    };\n}\n\n}  // namespace cases\n")

    print("wrote %d cases to %s" % (len(blocks), out))
    for why, count in skipped.most_common():
        print("  skipped %3d: %s" % (count, why))


if __name__ == "__main__":
    main(sys.argv[1:])
