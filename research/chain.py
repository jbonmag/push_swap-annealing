#!/usr/bin/env python3
"""Pipe a solver through the window superoptimizer so bench.py can verify the pair.
usage: chain.py SUPEROPT DEPTH SOLVER_CMD... -- num..."""
import subprocess
import sys


def main():
    sup, depth = sys.argv[1], sys.argv[2]
    cut = sys.argv.index("--")
    cmd = sys.argv[3:cut]
    nums = sys.argv[cut + 1:]
    first = subprocess.run(cmd + nums, capture_output=True, text=True)
    out = subprocess.run([sup, str(len(nums)), depth], input=first.stdout,
                         capture_output=True, text=True)
    sys.stdout.write(out.stdout)
    sys.exit(out.returncode)


main()
