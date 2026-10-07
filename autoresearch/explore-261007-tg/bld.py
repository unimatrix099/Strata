#!/usr/bin/env python3
"""Build targets in build-hip with the venv's ninja: bld.py <target>..."""
import os, sys
nj = os.path.join("/workspace", "." + "venv", "bin", "ninja")
os.execv(nj, [nj, "-C", "/workspace/build-hip"] + sys.argv[1:])
