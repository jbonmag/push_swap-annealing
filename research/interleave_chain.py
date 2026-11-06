#!/usr/bin/env python3
"""Run the engine, then let the interleave probe continue from its solution.
usage: interleave_chain.py ENGINE ENGINE_SECS INTERLEAVE SECS SEED num..."""
import os
import subprocess
import sys
import tempfile


def main():
    engine, esecs, inter, secs, seed = sys.argv[1:6]
    nums = sys.argv[6:]
    env = dict(os.environ, DUMP="1", PAIRS="0")
    first = subprocess.run([engine, esecs, seed] + nums, capture_output=True, text=True, env=env)
    rows = [ln for ln in first.stderr.splitlines() if len(ln.split()) == 3 and ln.replace(" ", "").isdigit()]
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write("\n".join(rows) + "\n")
        name = f.name
    out = subprocess.run([inter, secs, seed, name] + nums, capture_output=True, text=True)
    os.unlink(name)
    sys.stderr.write(out.stderr)
    sys.stdout.write(out.stdout)
    sys.exit(out.returncode)


main()
