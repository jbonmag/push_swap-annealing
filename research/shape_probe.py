#!/usr/bin/env python3
"""What do good solutions look like, and which starting shapes win?
usage: shape_probe.py ENGINE SECONDS N RUNS"""
import random
import re
import statistics as st
import subprocess
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor


def run(engine, secs, n, seed, dump):
    nums = random.Random(seed).sample(range(100000), n)
    env = {"DUMP": "1"} if dump else {}
    p = subprocess.run([engine, str(secs), "1"] + [str(x) for x in nums],
                       capture_output=True, text=True, env=env)
    return p.stderr.splitlines(), nums


def profile(seq, n, bins=25):
    size = max(1, len(seq) // bins)
    return " ".join(f"{100 * st.mean(seq[i:i + size]) / n:3.0f}"
                    for i in range(0, len(seq), size))


def main():
    engine, secs, n, runs = sys.argv[1], float(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
    with ThreadPoolExecutor(runs) as ex:
        res = list(ex.map(lambda s: run(engine, secs, n, 100 + s, True), range(runs)))
    wins = Counter()
    bests = []
    for lines, _ in res:
        m = re.search(r"k=(\d+) off=(\d+) rev=(\d+) best=(\d+).*keep=(\d+)", lines[-1] if "k=" in lines[-1] else lines[0])
        wins[f"k={m.group(1)}"] += 1
        wins[f"off={m.group(2)}"] += 1
        wins[f"rev={m.group(3)}"] += 1
        bests.append(int(m.group(4)))
    print(f"n={n} secs={secs} avg={st.mean(bests):.1f} winners={sorted(wins.items())}")
    for lines, _ in res[:4]:
        rows = [ln.split() for ln in lines if re.fullmatch(r"\d+ \d+ \d+", ln)]
        head = next(ln for ln in lines if "k=" in ln)
        print(head[:60])
        print(" sigma", profile([int(r[0]) for r in rows], n))
        print(" pi   ", profile([int(r[1]) for r in rows], n))
        print(" tau  ", profile([int(r[2]) for r in rows], n))


main()
