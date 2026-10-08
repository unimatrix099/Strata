#!/usr/bin/env python3
"""Resolve git conflict blocks in the given files by keeping the HEAD side (during a rebase: upstream + the commits
already replayed). Usage: take_ours_conflicts.py FILE..."""
import re, sys
pat = re.compile(r"<<<<<<< [^\n]*\n(.*?)=======\n.*?>>>>>>> [^\n]*\n", re.S)
for f in sys.argv[1:]:
    s = open(f).read()
    s2, n = pat.subn(lambda m: m.group(1), s)
    open(f, "w").write(s2)
    print(f, n, "blocks -> HEAD")
