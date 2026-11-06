#!/usr/bin/env python3
"""S9: guided beam search with a saved stage 2 model on sizes without exact
labels, against the research engine on the same inputs. A shorter valid
solution is simply better, so no labels are needed.

usage: s9_eval.py MODEL ENGINE N CASES WIDTH ENGINE_SECS
"""
import os
import random
import subprocess
import sys
import time

import numpy as np
import torch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from s9_stage2 import DEV, Model, beam  # noqa: E402


def engine_len(engine, secs, perm):
    env = dict(os.environ, ARMS="1", GAM="1,0.6,0.4")
    out = subprocess.run([engine, str(secs), "1"] + [str(v) for v in perm],
                         capture_output=True, text=True, env=env)
    return len(out.stdout.split())


def main():
    path, engine, n, cases, width, secs = sys.argv[1], sys.argv[2], \
        int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), float(sys.argv[6])
    model = Model().to(DEV)
    model.load_state_dict(torch.load(path, map_location=DEV))
    model.eval()
    rng = random.Random(11 + n)
    res = []
    for c in range(cases):
        p = list(range(n))
        rng.shuffle(p)
        t0 = time.monotonic()
        b = beam(model, (tuple(p), n), width, n)
        tb = time.monotonic() - t0
        e = engine_len(engine, secs, p)
        res.append((b, e))
        print(f"case {c}: beam={b} ({tb:.0f}s) engine={e}", flush=True)
    ok = [(b, e) for b, e in res if b is not None]
    print(f"n={n} width={width}: solved {len(ok)}/{cases} "
          f"beam={np.mean([b for b, _ in ok]):.2f} "
          f"engine={np.mean([e for _, e in ok]):.2f} "
          f"beam_wins={sum(b < e for b, e in ok)} ties={sum(b == e for b, e in ok)}")


if __name__ == "__main__":
    main()
