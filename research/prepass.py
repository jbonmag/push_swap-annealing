#!/usr/bin/env python3
"""Multi-pass probe: run fixed 3-way partition passes, then the engine on the
resulting stack. Each pass splits a by one base-3 digit of the rank:
digit 2 stays in a, digit 1 goes to b, digit 0 goes to the bottom of b; then b
returns to a, leaving a = low + mid + high. The last pass uses the most
significant digit.

usage: prepass.py PASSES ENGINE SECONDS SEED num...
"""
import subprocess
import sys
from collections import deque


def one_pass(a, n, passes, level):
    """Partition by the digit that matters at this level; returns (a, ops)."""
    div = n / 3 ** (passes - level)
    digit = lambda r: int(r / div) % 3
    remaining = sum(digit(r) != 2 for r in a)
    ops = []
    a, b = deque(a), deque()
    pending = False
    while remaining:
        d = digit(a[0])
        if d == 2:
            a.rotate(-1)
            ops.append("rr" if pending else "ra")
            if pending:
                b.rotate(-1)
                pending = False
            continue
        if pending:
            b.rotate(-1)
            ops.append("rb")
            pending = False
        b.appendleft(a.popleft())
        ops.append("pb")
        pending = d == 0
        remaining -= 1
    if pending:
        b.rotate(-1)
        ops.append("rb")
    while b:
        a.appendleft(b.popleft())
        ops.append("pa")
    return list(a), ops


def main():
    passes, engine, secs, seed = int(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]
    vals = [int(x) for x in sys.argv[5:]]
    order = {v: i for i, v in enumerate(sorted(vals))}
    a = [order[v] for v in vals]
    ops = []
    for level in range(passes):
        a, new = one_pass(a, len(a), passes, level)
        ops += new
    out = subprocess.run([engine, secs, seed] + [str(x) for x in a],
                         capture_output=True, text=True)
    print("\n".join(ops + out.stdout.split()))
    sys.exit(out.returncode)


main()
