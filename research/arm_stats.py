#!/usr/bin/env python3
"""Runs the engine on seeded inputs and tallies which starting shape won and
how good each family of starts was before annealing.

usage: arm_stats.py "ENV=.. ENGINE SECS SEED" N RUNS [SEED]
"""
import os
import random
import re
import shlex
import statistics as st
import subprocess
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor


def run(cmd, n, seed):
    parts = shlex.split(cmd)
    env = dict(os.environ)
    while parts and "=" in parts[0]:
        k, v = parts.pop(0).split("=", 1)
        env[k] = v
    nums = random.Random(seed).sample(range(100000), n)
    p = subprocess.run(parts + [str(x) for x in nums], capture_output=True, text=True, env=env)
    m = re.search(r"type=(\d+) k=(\d+) off=(\d+) rev=(\d+) gamma=([\d.]+) seed_bands=(\d+) "
                  r"seed_rings=(\d+) best=(\d+)", p.stderr)
    return m.groups() if m else None


def main():
    cmd, n, runs = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    seed = int(sys.argv[4]) if len(sys.argv) > 4 else 21
    with ThreadPoolExecutor(runs) as ex:
        res = [r for r in ex.map(lambda s: run(cmd, n, seed * 1000 + s), range(runs)) if r]
    wins = Counter(f"{'ring' if r[0] == '1' else 'bands'} k={r[1]} off={r[2]} "
                   f"{'desc' if r[0] == '1' and r[3] == '1' else 'rev' if r[3] == '1' else ''} g={r[4]}"
                   for r in res)
    sb = [int(r[5]) for r in res if int(r[5])]
    sr = [int(r[6]) for r in res if int(r[6])]
    print(f"n={n} final={st.mean(int(r[7]) for r in res):.1f} "
          f"seed_bands={st.mean(sb) if sb else 0:.1f} seed_rings={st.mean(sr) if sr else 0:.1f}")
    for k, v in wins.most_common():
        print(f"   {v:3d}  {k}")


main()
