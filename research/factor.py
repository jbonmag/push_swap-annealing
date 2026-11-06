#!/usr/bin/env python3
"""Multi-pass probe with both passes optimised: sort P as Q then Q^-1 P, where Q
is the intermediate order. Ops do not name values, so each factor is an
ordinary instance for the engine, relabelled by its target position.
The intermediate order groups ranks into K bands, keeping the initial order
inside each band. The time budget is split between the factors.

usage: factor.py K ENGINE SECONDS SEED num...
"""
import subprocess
import sys


def solve(engine, secs, seed, seq):
    out = subprocess.run([engine, str(secs), seed] + [str(x) for x in seq],
                         capture_output=True, text=True)
    if out.returncode:
        sys.exit(out.returncode)
    return out.stdout.split()


def main():
    k, engine, secs, seed = int(sys.argv[1]), sys.argv[2], float(sys.argv[3]), sys.argv[4]
    vals = [int(x) for x in sys.argv[5:]]
    n = len(vals)
    order = {v: i for i, v in enumerate(sorted(vals))}
    ranks = [order[v] for v in vals]
    mid = sorted(ranks, key=lambda r: r * k // n)
    target = {r: i for i, r in enumerate(mid)}
    first = solve(engine, secs / 2, seed, [target[r] for r in ranks])
    second = solve(engine, secs / 2, seed, mid)
    print(f"first={len(first)} second={len(second)}", file=sys.stderr)
    print("\n".join(first + second))


main()
