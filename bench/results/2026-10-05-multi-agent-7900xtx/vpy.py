#!/usr/bin/env python3
"""Run a script with the installation's own Python (the setup's virtual environment): vpy.py tools/batch_test.py ..."""
import os, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
py = os.path.join(ROOT, ".venv", "bin", "python")
os.execv(py, [py] + sys.argv[1:])
