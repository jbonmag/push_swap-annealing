#!/usr/bin/env python3
"""S9, stage 2: does a learned cost-to-go generalise to sizes it never saw?

Trains one model on the exact labels of several small n (dumps written by
`DUMP=path bfs_exact N 0 0`) and tests it on a larger n: error on held-out
states and beam search against the exact optimum. The model sees one token
per element with features that do not depend on n, mixes them with a small
transformer, and sums one contribution per token.

usage: s9_stage2.py DUMPDIR TRAIN_NS TEST_N [epochs] [beam_widths]
  e.g. s9_stage2.py ~/psw/s9 7,8,9 10 30 100,1000
"""
import math
import os
import random
import sys
import time

import numpy as np
import torch

DEV = torch.device("cuda" if torch.cuda.is_available() else "cpu")
NF = 13


def unrank_batch(r, n):
    fact = [math.factorial(i) for i in range(n + 1)]
    r = r.copy()
    used = np.zeros((len(r), n), dtype=bool)
    perm = np.empty((len(r), n), dtype=np.int64)
    for i in range(n):
        c = r // fact[n - 1 - i]
        r %= fact[n - 1 - i]
        free_rank = np.cumsum(~used, axis=1) - 1
        pick = np.argmax((~used) & (free_rank == c[:, None]), axis=1)
        perm[:, i] = pick
        used[np.arange(len(r)), pick] = True
    return perm


def rank_one(p):
    n = len(p)
    r = 0
    for i in range(n):
        c = sum(1 for j in range(i + 1, n) if p[j] < p[i])
        r += c * math.factorial(n - 1 - i)
    return r


def features(perm, k):
    """Per-element features that keep their meaning for any n."""
    b, n = perm.shape
    j = np.arange(n)[None, :].repeat(b, 0)
    kk = k[:, None]
    in_a = j < kk
    size = np.where(in_a, kk, n - kk)
    idx = np.where(in_a, j, j - kk)
    sz = np.maximum(size, 1)
    nxt = np.where(in_a, (idx + 1) % sz, kk + (idx + 1) % sz)
    rank = perm
    rnext = np.take_along_axis(perm, nxt, axis=1)
    delta = rnext - rank
    x = np.stack([
        in_a, idx / sz, idx / 10.0, (size - 1 - idx) / 10.0, size / n,
        size / 10.0, rank / (n - 1), rank / 10.0, delta / n, delta == 1,
        delta == -1, (delta % n) == 1, np.broadcast_to(kk / n, (b, n)),
    ], axis=2).astype(np.float32)
    return x


class Model(torch.nn.Module):
    def __init__(self, d=96, layers=3):
        super().__init__()
        self.inp = torch.nn.Linear(NF, d)
        enc = torch.nn.TransformerEncoderLayer(d, 4, 2 * d, dropout=0.0,
                                               batch_first=True)
        self.enc = torch.nn.TransformerEncoder(enc, layers)
        self.out = torch.nn.Linear(d, 1)

    def forward(self, x):
        return self.out(self.enc(self.inp(x))).squeeze(2).sum(1)


def apply(op, p, k):
    """The same move semantics as bfs_exact.c."""
    n = len(p)
    q = list(p)
    m = n - k
    if op in (0, 2) and k >= 2:
        q[0], q[1] = q[1], q[0]
    if op in (1, 2) and m >= 2:
        q[k], q[k + 1] = q[k + 1], q[k]
    if op == 3:
        if k == 0:
            return None
        q[0:k - 1] = p[1:k]
        q[k - 1] = p[0]
        return tuple(q), k - 1
    if op == 4:
        if m == 0:
            return None
        q[1:k + 1] = p[0:k]
        q[0] = p[k]
        return tuple(q), k + 1
    if op in (5, 7) and k >= 2:
        q[0:k] = q[1:k] + q[0:1]
    if op in (6, 7) and m >= 2:
        q[k:n] = q[k + 1:n] + q[k:k + 1]
    if op in (8, 10) and k >= 2:
        q[0:k] = q[k - 1:k] + q[0:k - 1]
    if op in (9, 10) and m >= 2:
        q[k:n] = q[n - 1:n] + q[k:n - 1]
    if tuple(q) == tuple(p):
        return None
    return tuple(q), k


def beam(model, start, width, n):
    goal = (tuple(range(n)), n)
    frontier = [start]
    seen = {start}
    for depth in range(1, 8 * n):
        children = []
        for p, k in frontier:
            for op in range(11):
                c = apply(op, p, k)
                if c is None or c in seen:
                    continue
                if c == goal:
                    return depth
                seen.add(c)
                children.append(c)
        if not children:
            return None
        perm = np.array([c[0] for c in children], dtype=np.int64)
        ks = np.array([c[1] for c in children], dtype=np.int64)
        h = []
        with torch.no_grad():
            for i in range(0, len(children), 8192):
                xb = torch.from_numpy(features(perm[i:i + 8192], ks[i:i + 8192]))
                h.append(model(xb.to(DEV)).cpu().numpy())
        order = np.argsort(np.concatenate(h), kind="stable")[:width]
        frontier = [children[i] for i in order]
    return None


def load(dumpdir, n, count, rng):
    dist = np.fromfile(f"{dumpdir}/d{n}.bin", dtype=np.uint8)
    idx = rng.choice(len(dist), size=min(count, len(dist)), replace=False)
    perm = unrank_batch(idx // (n + 1), n)
    k = idx % (n + 1)
    return (torch.from_numpy(features(perm, k)).to(DEV),
            torch.from_numpy(dist[idx].astype(np.float32)).to(DEV), dist)


def evaluate(model, x, y):
    with torch.no_grad():
        p = torch.cat([model(x[i:i + 8192]) for i in range(0, len(x), 8192)])
    return (p - y).abs().mean().item(), ((p.round() - y).abs() < 0.5).float().mean().item()


def main():
    dumpdir = sys.argv[1]
    train_ns = [int(v) for v in sys.argv[2].split(",")]
    test_n = int(sys.argv[3])
    epochs = int(sys.argv[4]) if len(sys.argv) > 4 else 30
    widths = [int(w) for w in sys.argv[5].split(",")] if len(sys.argv) > 5 else [100, 1000]
    rng = np.random.default_rng(1)
    train = {n: load(dumpdir, n, 1_500_000, rng) for n in train_ns}
    xt, yt, dist_test = load(dumpdir, test_n, 200_000, rng)
    torch.manual_seed(1)
    model = Model().to(DEV)
    opt = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=1e-4)
    steps = sum(len(train[n][1]) for n in train_ns) // 2048
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, 1e-3, total_steps=epochs * steps)
    t0 = time.monotonic()
    for ep in range(epochs):
        batches = [(n, i) for n in train_ns for i in range(0, len(train[n][1]) - 2047, 2048)]
        random.Random(ep).shuffle(batches)
        for n, i in batches[:steps]:
            x, y, _ = train[n]
            sel = torch.randint(0, len(y), (2048,), device=DEV)
            loss = torch.nn.functional.smooth_l1_loss(model(x[sel]), y[sel])
            opt.zero_grad()
            loss.backward()
            opt.step()
            sched.step()
        if ep % 5 == 4 or ep == epochs - 1:
            parts = []
            for n in train_ns:
                mae, ex = evaluate(model, train[n][0][:100_000], train[n][1][:100_000])
                parts.append(f"n={n} mae={mae:.3f} exact={ex:.3f}")
            mae, ex = evaluate(model, xt, yt)
            parts.append(f"TEST n={test_n} mae={mae:.3f} exact={ex:.3f}")
            print(f"epoch {ep} " + " | ".join(parts) + f" t={time.monotonic() - t0:.0f}s",
                  flush=True)
    model.eval()
    if os.environ.get("SAVE"):
        torch.save(model.state_dict(), os.environ["SAVE"])
    random.seed(7)
    starts = []
    for _ in range(200):
        p = list(range(test_n))
        random.shuffle(p)
        starts.append((tuple(p), test_n))
    opt_len = [int(dist_test[rank_one(p) * (test_n + 1) + test_n]) for p, _ in starts]
    print(f"optimum over these starts: {np.mean(opt_len):.3f}")
    for width in widths:
        t1 = time.monotonic()
        res = [beam(model, s, width, test_n) for s in starts]
        ok = [r for r in res if r is not None]
        gap = [r - o for r, o in zip(res, opt_len) if r is not None]
        print(f"beam {width}: avg={np.mean(ok) if ok else float('nan'):.3f} "
              f"solved={len(ok)}/200 above_opt={np.mean(gap) if gap else float('nan'):.3f} "
              f"t={time.monotonic() - t1:.0f}s", flush=True)


if __name__ == "__main__":
    main()
