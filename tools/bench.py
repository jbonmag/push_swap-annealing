#!/usr/bin/env python3
"""Seeded, verified benchmark for push_swap solvers.

A solver is a command string; the numbers are appended as arguments, so
"./push_swap" and "/tmp/ps/engine 60 1" both work. Every output is replayed
by an independent checker, and an invalid output is always reported.

  bench.py run CMD N RUNS [--seed S] [--jobs J]
  bench.py compare CMD_A CMD_B N RUNS [--seed S] [--jobs J]
  bench.py exhaustive CMD N
  bench.py gate CMD_A CMD_B quick|confirm

gate is the routine test for a new idea: A is the reference, B the candidate,
compared on the same instances at n=100 and n=500. In its commands {t} is the
time budget and {w} the engine build for that size. Each level has its own
instances, so confirm also catches ideas tuned to the quick set.
"""
import argparse
import itertools
import math
import random
import shlex
import statistics as st
import subprocess
import sys
import time
from collections import deque
from concurrent.futures import ThreadPoolExecutor


def swap(d):
    if len(d) > 1:
        d[0], d[1] = d[1], d[0]


def rot(d):
    if d:
        d.rotate(-1)


def rrot(d):
    if d:
        d.rotate(1)


def is_valid(nums, ops):
    a, b = deque(nums), deque()
    for op in ops:
        if op == "sa":
            swap(a)
        elif op == "sb":
            swap(b)
        elif op == "ss":
            swap(a)
            swap(b)
        elif op == "pa":
            if b:
                a.appendleft(b.popleft())
        elif op == "pb":
            if a:
                b.appendleft(a.popleft())
        elif op == "ra":
            rot(a)
        elif op == "rb":
            rot(b)
        elif op == "rr":
            rot(a)
            rot(b)
        elif op == "rra":
            rrot(a)
        elif op == "rrb":
            rrot(b)
        elif op == "rrr":
            rrot(a)
            rrot(b)
        else:
            return False
    return not b and list(a) == sorted(nums)


def solve(cmd, nums):
    start = time.monotonic()
    out = subprocess.run(shlex.split(cmd) + [str(x) for x in nums],
                         capture_output=True, text=True)
    elapsed = time.monotonic() - start
    ops = out.stdout.split()
    return len(ops), elapsed, out.returncode == 0 and is_valid(nums, ops)


def instances(n, runs, seed):
    rng = random.Random(seed * 1000003 + n)
    return [rng.sample(range(100000), n) for _ in range(runs)]


def summary(label, res):
    counts = [r[0] for r in res]
    times = [r[1] for r in res]
    bad = sum(not r[2] for r in res)
    print(f"{label}: avg={st.mean(counts):.1f} sd={st.pstdev(counts):.1f} "
          f"min={min(counts)} max={max(counts)} | "
          f"t_avg={st.mean(times):.2f}s t_max={max(times):.2f}s | "
          f"invalid={bad}/{len(res)}", flush=True)
    return bad


def cmd_run(args):
    cases = instances(args.n, args.runs, args.seed)
    with ThreadPoolExecutor(args.jobs) as ex:
        res = list(ex.map(lambda c: solve(args.cmd, c), cases))
    return 1 if summary(f"n={args.n} seed={args.seed} [{args.cmd}]", res) else 0


def cmd_compare(args):
    cases = instances(args.n, args.runs, args.seed)
    with ThreadPoolExecutor(args.jobs) as ex:
        ra = list(ex.map(lambda c: solve(args.cmd_a, c), cases))
        rb = list(ex.map(lambda c: solve(args.cmd_b, c), cases))
    bad = summary(f"A [{args.cmd_a}]", ra) + summary(f"B [{args.cmd_b}]", rb)
    diffs = [y[0] - x[0] for x, y in zip(ra, rb)]
    half = 1.96 * st.stdev(diffs) / math.sqrt(len(diffs)) if len(diffs) > 1 else 0
    print(f"B-A: mean={st.mean(diffs):+.1f} ci95=+-{half:.1f} "
          f"B_wins={sum(d < 0 for d in diffs)}/{len(diffs)}", flush=True)
    return 1 if bad else 0


GATE_LEVELS = {"quick": (5, 7), "confirm": (30, 8)}
GATE_RUNS = 20


def cmd_gate(args):
    budget, seed = GATE_LEVELS[args.level]
    failed = 0
    for n in (100, 500):
        build = 2 if n <= 128 else 8
        args.cmd_a = args.a.format(t=budget, w=build)
        args.cmd_b = args.b.format(t=budget, w=build)
        args.n, args.runs, args.seed = n, GATE_RUNS, seed
        print(f"--- {args.level} n={n} budget={budget}s", flush=True)
        failed |= cmd_compare(args)
    return failed


def cmd_exhaustive(args):
    cases = [list(p) for p in itertools.permutations(range(args.n))]
    with ThreadPoolExecutor(args.jobs) as ex:
        res = list(ex.map(lambda c: solve(args.cmd, c), cases))
    return 1 if summary(f"all perms n={args.n} [{args.cmd}]", res) else 0


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="mode", required=True)
    run = sub.add_parser("run")
    run.add_argument("cmd")
    cmp_ = sub.add_parser("compare")
    cmp_.add_argument("cmd_a")
    cmp_.add_argument("cmd_b")
    for p in (run, cmp_):
        p.add_argument("n", type=int)
        p.add_argument("runs", type=int)
        p.add_argument("--seed", type=int, default=1)
    exh = sub.add_parser("exhaustive")
    exh.add_argument("cmd")
    exh.add_argument("n", type=int)
    gate = sub.add_parser("gate")
    gate.add_argument("a")
    gate.add_argument("b")
    gate.add_argument("level", choices=sorted(GATE_LEVELS))
    for p in (run, cmp_, exh):
        p.add_argument("--jobs", type=int, default=8)
    gate.add_argument("--jobs", type=int, default=GATE_RUNS)
    args = parser.parse_args()
    sys.exit({"run": cmd_run, "compare": cmd_compare, "gate": cmd_gate,
              "exhaustive": cmd_exhaustive}[args.mode](args))


main()
