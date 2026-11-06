#!/usr/bin/env python3
"""Where do the rotations go? Splits a solver's output into steps (the
rotations before each push) and reports how the rotations spread over steps.

usage: step_hist.py SOLVER_CMD N RUNS [SEED]
"""
import random
import shlex
import subprocess
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor

ROT = {"ra", "rb", "rr", "rra", "rrb", "rrr", "sa", "sb", "ss"}


def steps(ops):
    """Yields (phase, rotations before the push) for every push."""
    cur = 0
    for op in ops:
        if op in ROT:
            cur += 1
        else:
            yield op, cur
            cur = 0


def run(cmd, n, seed):
    nums = random.Random(seed).sample(range(100000), n)
    out = subprocess.run(shlex.split(cmd) + [str(x) for x in nums],
                         capture_output=True, text=True).stdout.split()
    return out


def main():
    cmd, n, runs = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    seed = int(sys.argv[4]) if len(sys.argv) > 4 else 11
    with ThreadPoolExecutor(runs) as ex:
        outs = list(ex.map(lambda s: run(cmd, n, seed * 1000 + s), range(runs)))
    for phase in ("pb", "pa"):
        hist = Counter()
        for ops in outs:
            for ph, r in steps(ops):
                if ph == phase:
                    hist[r] += 1
        nsteps = sum(hist.values())
        rots = sum(r * c for r, c in hist.items())
        print(f"{phase}: steps={nsteps / runs:.1f} rot={rots / runs:.1f} "
              f"rot/step={rots / max(nsteps, 1):.2f}")
        acc_s = acc_r = 0
        for lo, hi in ((0, 0), (1, 1), (2, 2), (3, 4), (5, 8), (9, 16), (17, 32), (33, 10**9)):
            s = sum(c for r, c in hist.items() if lo <= r <= hi)
            rr = sum(r * c for r, c in hist.items() if lo <= r <= hi)
            acc_s += s
            acc_r += rr
            label = f"{lo}" if lo == hi else f"{lo}-{hi if hi < 10**9 else '...'}"
            print(f"   rot {label:>6}: {100 * s / max(nsteps, 1):5.1f}% pasos  "
                  f"{100 * rr / max(rots, 1):5.1f}% rotaciones")
    tail = 0
    for ops in outs:
        for ph, r in steps(ops):
            if r > 4:
                tail += r - 4
    print(f"rotaciones por encima de 4 en un paso (cota de ahorro con segundas salidas a +2): {tail / runs:.1f} por caso")


main()
