#!/usr/bin/env python3
"""S9, stage 1: can a learned cost-to-go guide a search to near-optimal
solutions when its labels are exact?

Reads the exact cost-to-go of every n=10 state written by
`DUMP=path bfs_exact 10 0 0` (one byte per state, indexed by
rank_perm * (n + 1) + k, k = size of a; the state array holds a top to bottom,
then b top to bottom). Trains an MLP on a sample, then runs beam search with
it on random starts and compares the lengths with the exact optimum.

usage: s9_stage1.py DUMPFILE [train_samples] [epochs] [beam_widths]
"""
import math
import random
import sys
import time

import numpy as np
import torch

N = 10
FACT = [math.factorial(i) for i in range(N + 1)]
DEV = torch.device("cuda" if torch.cuda.is_available() else "cpu")
OPS = ["sa", "sb", "ss", "pb", "pa", "ra", "rb", "rr", "rra", "rrb", "rrr"]


def unrank_batch(r):
    """Lehmer decode of many permutation ranks at once."""
    r = r.copy()
    used = np.zeros((len(r), N), dtype=bool)
    perm = np.empty((len(r), N), dtype=np.int64)
    for i in range(N):
        c = r // FACT[N - 1 - i]
        r %= FACT[N - 1 - i]
        free_rank = np.cumsum(~used, axis=1) - 1
        pick = np.argmax((~used) & (free_rank == c[:, None]), axis=1)
        perm[:, i] = pick
        used[np.arange(len(r)), pick] = True
    return perm


def rank_one(p):
    r = 0
    for i in range(N):
        c = sum(1 for j in range(i + 1, N) if p[j] < p[i])
        r += c * FACT[N - 1 - i]
    return r


def features(perm, k):
    """One-hot (position, rank), plus whether each position is in a."""
    b = len(perm)
    x = np.zeros((b, N * N + N), dtype=np.float32)
    rows = np.repeat(np.arange(b), N)
    cols = (np.tile(np.arange(N), b) * N + perm.reshape(-1))
    x[rows, cols] = 1.0
    x[:, N * N:] = (np.arange(N)[None, :] < k[:, None]).astype(np.float32)
    return x


def apply(op, p, k):
    """The same move semantics as bfs_exact.c."""
    q = list(p)
    m = N - k
    if op in (0, 2) and k >= 2:
        q[0], q[1] = q[1], q[0]
    if op in (1, 2) and m >= 2:
        q[k], q[k + 1] = q[k + 1], q[k]
    if op == 3:
        if k == 0:
            return None
        t = q[0]
        q[0:k - 1] = p[1:k]
        q[k - 1] = t
        return tuple(q), k - 1
    if op == 4:
        if m == 0:
            return None
        t = p[k]
        q[1:k + 1] = p[0:k]
        q[0] = t
        return tuple(q), k + 1
    if op in (5, 7) and k >= 2:
        q[0:k] = q[1:k] + q[0:1]
    if op in (6, 7) and m >= 2:
        q[k:N] = q[k + 1:N] + q[k:k + 1]
    if op in (8, 10) and k >= 2:
        q[0:k] = q[k - 1:k] + q[0:k - 1]
    if op in (9, 10) and m >= 2:
        q[k:N] = q[N - 1:N] + q[k:N - 1]
    if tuple(q) == tuple(p):
        return None
    return tuple(q), k


def beam(model, start, width):
    goal = (tuple(range(N)), N)
    frontier = [start]
    seen = {start}
    for depth in range(1, 60):
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
        with torch.no_grad():
            h = model(torch.from_numpy(features(perm, ks)).to(DEV))
            h = h.squeeze(1).cpu().numpy()
        order = np.argsort(h, kind="stable")[:width]
        frontier = [children[i] for i in order]
    return None


def main():
    path = sys.argv[1]
    nsamp = int(sys.argv[2]) if len(sys.argv) > 2 else 3_000_000
    epochs = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    dist = np.fromfile(path, dtype=np.uint8)
    assert len(dist) == FACT[N] * (N + 1)
    rng = np.random.default_rng(1)
    idx = rng.choice(len(dist), size=nsamp + 200_000, replace=False)
    perm = unrank_batch(idx // (N + 1))
    k = idx % (N + 1)
    x = torch.from_numpy(features(perm, k)).to(DEV)
    y = torch.from_numpy(dist[idx].astype(np.float32)).to(DEV)
    print(f"device {DEV}", flush=True)
    xt, yt, xv, yv = x[:nsamp], y[:nsamp], x[nsamp:], y[nsamp:]
    torch.manual_seed(1)
    model = torch.nn.Sequential(
        torch.nn.Linear(N * N + N, 512), torch.nn.ReLU(),
        torch.nn.Linear(512, 512), torch.nn.ReLU(),
        torch.nn.Linear(512, 256), torch.nn.ReLU(),
        torch.nn.Linear(256, 1)).to(DEV)
    opt = torch.optim.Adam(model.parameters(), lr=1e-3)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, epochs)
    t0 = time.monotonic()
    for ep in range(epochs):
        perm_t = torch.randperm(nsamp, device=DEV)
        for i in range(0, nsamp, 4096):
            b = perm_t[i:i + 4096]
            loss = torch.nn.functional.mse_loss(model(xt[b]).squeeze(1), yt[b])
            opt.zero_grad()
            loss.backward()
            opt.step()
        sched.step()
        with torch.no_grad():
            pv = model(xv).squeeze(1)
            mae = (pv - yv).abs().mean().item()
            exact = ((pv.round() - yv).abs() < 0.5).float().mean().item()
        print(f"epoch {ep} val_mae={mae:.3f} rounded_exact={exact:.3f} "
              f"t={time.monotonic() - t0:.0f}s", flush=True)
    model.eval()
    random.seed(7)
    starts = []
    for _ in range(200):
        p = list(range(N))
        random.shuffle(p)
        starts.append((tuple(p), N))
    opt_len = [int(dist[rank_one(p) * (N + 1) + N]) for p, _ in starts]
    print(f"optimum over these starts: {np.mean(opt_len):.3f}")
    widths = [int(w) for w in sys.argv[4].split(",")] if len(sys.argv) > 4 \
        else [1, 10, 100]
    for width in widths:
        t1 = time.monotonic()
        res = [beam(model, s, width) for s in starts]
        ok = [r for r in res if r is not None]
        gap = np.mean([r - o for r, o in zip(res, opt_len) if r is not None])
        print(f"beam {width}: avg={np.mean(ok):.3f} solved={len(ok)}/200 "
              f"above_opt={gap:.3f} t={time.monotonic() - t1:.0f}s", flush=True)


main()
