#!/usr/bin/env python3
"""Solve the inverse permutation and turn its solution around.

Every op has an inverse in the op set. If S sorts the arrangement A = B^-1,
then S reversed, with each op replaced by its inverse, sorts B, and it is just
as long. The engine therefore sees a different instance with the same answer
length, where the starting shapes mean something else: bands by position
instead of by rank.

usage: dual.py MODE ENGINE SECONDS SEED num...
  MODE dual: only the inverse. best: half the time on each, keep the shorter.
"""
import os
import subprocess
import sys

INVERSE = {"sa": "sa", "sb": "sb", "ss": "ss", "pa": "pb", "pb": "pa",
           "ra": "rra", "rb": "rrb", "rr": "rrr", "rra": "ra", "rrb": "rb", "rrr": "rr"}


def solve(engine, secs, seed, nums):
    out = subprocess.run([engine, str(secs), seed] + [str(x) for x in nums],
                         capture_output=True, text=True, env=os.environ)
    if out.returncode:
        sys.exit(out.returncode)
    return out.stdout.split()


def main():
    mode, engine, secs, seed = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4]
    vals = [int(x) for x in sys.argv[5:]]
    order = {v: i for i, v in enumerate(sorted(vals))}
    b = [order[v] for v in vals]
    a = [0] * len(b)
    for pos, rank in enumerate(b):
        a[rank] = pos
    if mode == "dual":
        ops = [INVERSE[o] for o in reversed(solve(engine, secs, seed, a))]
    else:
        direct = solve(engine, secs / 2, seed, b)
        turned = [INVERSE[o] for o in reversed(solve(engine, secs / 2, seed, a))]
        ops = direct if len(direct) <= len(turned) else turned
    print("\n".join(ops))


main()
