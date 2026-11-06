#!/usr/bin/env python3
"""S9, stage 3: a learned cost-to-go at n=100, trained on engine solutions.

  gen   ENGINE N COUNT SECS OUT.npz   run the engine on COUNT random inputs and
        label every state of each solution with the operations still to go
  train DATA.npz [DATA2.npz ...] OUT.pt EPOCHS
  dagger MODEL.pt BASE.npz ENGINE N ROUNDS OUTDIR
        rounds of guided search, labelling of the states it reaches by the
        finisher, and fine-tuning; see cmd_dagger
  eval  MODEL.pt ENGINE N CASES WIDTH ENGINE_SECS [OUT.npz]
        guided beam search against the engine on new inputs; with OUT.npz
        the beam solutions are saved as labelled data for the next round

States are stored as uint16 ranks (a top to bottom, then b top to bottom)
plus k, the size of a; features are computed on the GPU.
"""
import os
import random
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import torch

DEV = torch.device("cuda" if torch.cuda.is_available() else "cpu")
NF = 13
OPI = {"sa": 0, "sb": 1, "ss": 2, "pb": 3, "pa": 4, "ra": 5, "rb": 6, "rr": 7,
       "rra": 8, "rrb": 9, "rrr": 10}
ENV = dict(os.environ, ARMS="1", GAM="1,0.6,0.4")


def feats(perm, k):
    """perm: B x n long tensor, k: B long tensor, both on DEV."""
    b, n = perm.shape
    j = torch.arange(n, device=perm.device).expand(b, n)
    kk = k[:, None]
    in_a = j < kk
    size = torch.where(in_a, kk, n - kk)
    idx = torch.where(in_a, j, j - kk)
    sz = size.clamp(min=1)
    nxt = torch.where(in_a, (idx + 1) % sz, kk + (idx + 1) % sz)
    delta = torch.gather(perm, 1, nxt) - perm
    f = [in_a, idx / sz, idx / 10.0, (size - 1 - idx) / 10.0, size / n,
         size / 10.0, perm / (n - 1), perm / 10.0, delta / n, delta == 1,
         delta == -1, torch.remainder(delta, n) == 1, (kk / n).expand(b, n)]
    return torch.stack([t.float() for t in f], dim=2)


class Model(torch.nn.Module):
    def __init__(self, d=96, layers=3):
        super().__init__()
        self.inp = torch.nn.Linear(NF, d)
        enc = torch.nn.TransformerEncoderLayer(d, 4, 2 * d, dropout=0.0,
                                               batch_first=True)
        self.enc = torch.nn.TransformerEncoder(enc, layers)
        self.out = torch.nn.Linear(d, 1)

    def forward(self, perm, k):
        return self.out(self.enc(self.inp(feats(perm, k)))).squeeze(2).sum(1)


def apply(op, p, k):
    """Move semantics of bfs_exact.c on a tuple state; None if no change."""
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


def run_engine(engine, secs, vals, seed=1):
    out = subprocess.run([engine, str(secs), str(seed)] + [str(v) for v in vals],
                         capture_output=True, text=True, env=ENV)
    return out.stdout.split()


def trajectory(ranks, ops):
    """Every state along a solution, labelled with the operations still to go;
    None if the solution does not sort."""
    s = (tuple(ranks), len(ranks))
    states = [s]
    for o in ops:
        c = apply(OPI[o], *s)
        if c is None:
            c = s
        s = c
        states.append(s)
    if s != (tuple(range(len(ranks))), len(ranks)):
        return None
    total = len(ops)
    return [(st, total - i) for i, st in enumerate(states)]


def pack(rows, out):
    perm = np.array([r[0][0] for r in rows], dtype=np.uint16)
    k = np.array([r[0][1] for r in rows], dtype=np.uint16)
    y = np.array([r[1] for r in rows], dtype=np.uint16)
    np.savez_compressed(out, perm=perm, k=k, y=y)
    return len(rows)


def cmd_gen(engine, n, count, secs, out):
    rng = random.Random(1000 + n)
    cases = []
    for _ in range(count):
        vals = rng.sample(range(-99999, 99999), n)
        order = sorted(vals)
        cases.append((vals, [order.index(v) for v in vals]))
    t0 = time.monotonic()
    with ThreadPoolExecutor(20) as ex:
        sols = list(ex.map(lambda c: run_engine(engine, secs, c[0]), cases))
    rows, lens = [], []
    for (vals, ranks), ops in zip(cases, sols):
        tr = trajectory(ranks, ops)
        if tr is None:
            print("invalid engine output skipped", flush=True)
            continue
        rows.extend(tr)
        lens.append(len(ops))
    print(f"gen: {len(lens)} solutions, mean length {np.mean(lens):.1f}, "
          f"{pack(rows, out)} states, {time.monotonic() - t0:.0f}s", flush=True)


def cmd_train(paths, out, epochs):
    data = [np.load(p) for p in paths]
    perm = torch.from_numpy(np.concatenate([d["perm"] for d in data]).astype(np.int64)).to(DEV)
    k = torch.from_numpy(np.concatenate([d["k"] for d in data]).astype(np.int64)).to(DEV)
    y = torch.from_numpy(np.concatenate([d["y"] for d in data]).astype(np.float32)).to(DEV)
    nval = min(50_000, len(y) // 10)
    g = torch.Generator(device="cpu").manual_seed(1)
    order = torch.randperm(len(y), generator=g).to(DEV)
    val, tr = order[:nval], order[nval:]
    torch.manual_seed(1)
    model = Model().to(DEV)
    opt = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=1e-4)
    bs = 512
    steps = len(tr) // bs
    sched = torch.optim.lr_scheduler.OneCycleLR(opt, 1e-3, total_steps=epochs * steps)
    t0 = time.monotonic()
    for ep in range(epochs):
        sh = tr[torch.randperm(len(tr), device=DEV)]
        model.train()
        for i in range(steps):
            b = sh[i * bs:(i + 1) * bs]
            loss = torch.nn.functional.smooth_l1_loss(model(perm[b], k[b]), y[b])
            opt.zero_grad()
            loss.backward()
            opt.step()
            sched.step()
        model.eval()
        with torch.no_grad():
            pv = torch.cat([model(perm[val[i:i + 2048]], k[val[i:i + 2048]])
                            for i in range(0, nval, 2048)])
            mae = (pv - y[val]).abs().mean().item()
        print(f"epoch {ep} val_mae={mae:.2f} t={time.monotonic() - t0:.0f}s", flush=True)
        torch.save(model.state_dict(), out)


def beam(model, start, width, n, collect=None, stall=None):
    """Beam search guided by the model. With collect, every 15 levels the
    best state and a random one of the frontier are appended to it. With
    stall, it gives up when the best estimate has not dropped by 1 in that
    many levels."""
    goal = (tuple(range(n)), n)
    frontier = [(start, None)]
    seen = {start}
    hist = []
    best_h, best_at = float("inf"), 0
    for depth in range(1, 8 * n):
        children = []
        for fi, (s, _) in enumerate(frontier):
            for op in range(11):
                c = apply(op, *s)
                if c is None or c in seen:
                    continue
                if c == goal:
                    path = [op]
                    back = fi
                    for layer in reversed(hist):
                        path.append(layer[back][1])
                        back = layer[back][0]
                    return depth, path[::-1]
                seen.add(c)
                children.append((c, (fi, op)))
        if not children:
            return None, None
        perm = torch.tensor([c[0][0] for c in children], dtype=torch.long, device=DEV)
        ks = torch.tensor([c[0][1] for c in children], dtype=torch.long, device=DEV)
        with torch.no_grad():
            h = torch.cat([model(perm[i:i + 4096], ks[i:i + 4096])
                           for i in range(0, len(children), 4096)])
        top = torch.argsort(h, stable=True)[:width].tolist()
        hist.append([children[i][1] for i in top])
        frontier = [(children[i][0], children[i][1]) for i in top]
        if collect is not None and depth % 15 == 0:
            collect.append(frontier[0][0])
            collect.append(frontier[random.randrange(len(frontier))][0])
        hb = h[top[0]].item()
        if hb < best_h - 1:
            best_h, best_at = hb, depth
        if stall and depth - best_at > stall:
            return None, None
    return None, None


def cmd_eval(path, engine, n, cases, width, secs, out=None):
    model = Model().to(DEV)
    model.load_state_dict(torch.load(path, map_location=DEV))
    model.eval()
    rng = random.Random(77 + n)
    names = {v: o for o, v in OPI.items()}
    rows, res = [], []
    for c in range(cases):
        vals = rng.sample(range(-99999, 99999), n)
        order = sorted(vals)
        ranks = [order.index(v) for v in vals]
        t0 = time.monotonic()
        d, path = beam(model, (tuple(ranks), n), width, n)
        tb = time.monotonic() - t0
        e = len(run_engine(engine, secs, vals))
        if path is not None:
            tr = trajectory(ranks, [names[o] for o in path])
            assert tr is not None and len(path) == d
            rows.extend(tr)
        res.append((d, e))
        print(f"case {c}: beam={d} ({tb:.0f}s) engine={e}", flush=True)
    ok = [(b, e) for b, e in res if b is not None]
    if ok:
        print(f"n={n} width={width}: solved {len(ok)}/{cases} "
              f"beam={np.mean([b for b, _ in ok]):.1f} engine={np.mean([e for _, e in ok]):.1f} "
              f"beam_wins={sum(b < e for b, e in ok)}", flush=True)
    else:
        print(f"n={n} width={width}: solved 0/{cases}", flush=True)
    if out and rows:
        pack(rows, out)


def finish(engine, secs, state, seed):
    """An upper bound from any state: pull all of b back onto a, then solve
    that permutation with the engine. Returns the labelled path, or None."""
    p, k = state
    ops = ["pa"] * (len(p) - k)
    s = state
    for _ in ops:
        s = apply(4, *s)
    sol = run_engine(engine, secs, list(s[0]), seed)
    full = ops + sol
    cur = state
    rows = [(cur, len(full))]
    for i, o in enumerate(full):
        c = apply(OPI[o], *cur)
        cur = c if c is not None else cur
        rows.append((cur, len(full) - i - 1))
    if cur != (tuple(range(len(p))), len(p)):
        return None
    return rows


def to_gpu(rows_or_npz):
    if isinstance(rows_or_npz, str):
        d = np.load(rows_or_npz)
        perm, k, y = d["perm"], d["k"], d["y"]
    else:
        perm = np.array([r[0][0] for r in rows_or_npz], dtype=np.uint16)
        k = np.array([r[0][1] for r in rows_or_npz], dtype=np.uint16)
        y = np.array([r[1] for r in rows_or_npz], dtype=np.uint16)
    return (torch.from_numpy(perm.astype(np.int16)).to(DEV),
            torch.from_numpy(k.astype(np.int16)).to(DEV),
            torch.from_numpy(y.astype(np.float32)).to(DEV))


def fine_tune(model, sets, weights, steps, lr=3e-4, bs=1024):
    """Steps of AdamW on batches drawn from the data sets in proportion
    to weights, in bf16."""
    opt = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-4)
    model.train()
    w = torch.tensor(weights, dtype=torch.float)
    for _ in range(steps):
        parts = torch.multinomial(w, bs, replacement=True).bincount(minlength=len(sets))
        pb, kb, yb = [], [], []
        for (perm, k, y), cnt in zip(sets, parts.tolist()):
            if cnt:
                sel = torch.randint(0, len(y), (cnt,), device=DEV)
                pb.append(perm[sel])
                kb.append(k[sel])
                yb.append(y[sel])
        pb = torch.cat(pb).long()
        kb = torch.cat(kb).long()
        yb = torch.cat(yb)
        with torch.autocast("cuda", dtype=torch.bfloat16, enabled=DEV.type == "cuda"):
            pred = model(pb, kb)
        loss = torch.nn.functional.smooth_l1_loss(pred.float(), yb)
        opt.zero_grad()
        loss.backward()
        opt.step()
    model.eval()


def cmd_dagger(model_path, base, engine, n, rounds, outdir):
    """Rounds of: guided search on new inputs, label the states it reaches
    with the finisher, fine-tune on base data plus everything collected so
    far, and measure on fixed test inputs against the engine at 30 s."""
    model = Model().to(DEV)
    model.load_state_dict(torch.load(model_path, map_location=DEV))
    model.eval()
    sets = [to_gpu(base)]
    names = {v: o for o, v in OPI.items()}
    trng = random.Random(77 + n)
    tests = []
    for _ in range(5):
        vals = trng.sample(range(-99999, 99999), n)
        order = sorted(vals)
        tests.append([order.index(v) for v in vals])
    with ThreadPoolExecutor(5) as ex:
        eng = list(ex.map(lambda r: len(run_engine(engine, 30, r)), tests))
    print(f"test inputs, engine at 30 s: {eng} mean {np.mean(eng):.1f}", flush=True)
    drng = random.Random(500)
    for rnd in range(rounds):
        t0 = time.monotonic()
        states = []
        for _ in range(40):
            r = list(range(n))
            drng.shuffle(r)
            beam(model, (tuple(r), n), 50, n, collect=states, stall=120)
        t1 = time.monotonic()
        with ThreadPoolExecutor(20) as ex:
            paths = list(ex.map(lambda a: finish(engine, 0.3, a[1], a[0] + 1),
                                enumerate(states)))
        rows = [row for path in paths if path for row in path]
        labels = [path[0][1] for path in paths if path]
        sets.append(to_gpu(rows))
        np.savez_compressed(f"{outdir}/dag{rnd}.npz",
                            perm=np.array([r[0][0] for r in rows], dtype=np.uint16),
                            k=np.array([r[0][1] for r in rows], dtype=np.uint16),
                            y=np.array([r[1] for r in rows], dtype=np.uint16))
        with torch.no_grad():
            p0 = torch.tensor([s[0] for s in states], device=DEV)
            k0 = torch.tensor([s[1] for s in states], device=DEV)
            pred = model(p0, k0).cpu().numpy()
        under = np.mean(np.array(labels) - pred[:len(labels)])
        t2 = time.monotonic()
        weights = [0.5] + [0.5 / (len(sets) - 1)] * (len(sets) - 1)
        fine_tune(model, sets, weights, 3000)
        torch.save(model.state_dict(), f"{outdir}/dag{rnd}.pt")
        t3 = time.monotonic()
        res = []
        for r in tests:
            d, path = beam(model, (tuple(r), n), 100, n, stall=150)
            if path is not None:
                assert trajectory(r, [names[o] for o in path]) is not None
            res.append(d)
        ok = [(d, e) for d, e in zip(res, eng) if d is not None]
        msg = (f"beam={np.mean([d for d, _ in ok]):.1f} vs engine={np.mean([e for _, e in ok]):.1f}"
               if ok else "")
        print(f"round {rnd}: collected {len(states)} states, labelled {len(labels)} "
              f"(label minus model {under:+.1f}), {len(rows)} new rows | "
              f"test solved {len(ok)}/5 {msg} {res} | search {t1 - t0:.0f}s "
              f"label {t2 - t1:.0f}s train {t3 - t2:.0f}s test {time.monotonic() - t3:.0f}s",
              flush=True)


def main():
    mode = sys.argv[1]
    a = sys.argv[2:]
    if mode == "gen":
        cmd_gen(a[0], int(a[1]), int(a[2]), float(a[3]), a[4])
    elif mode == "train":
        cmd_train(a[:-2], a[-2], int(a[-1]))
    elif mode == "dagger":
        cmd_dagger(a[0], a[1], a[2], int(a[3]), int(a[4]), a[5])
    elif mode == "eval":
        cmd_eval(a[0], a[1], int(a[2]), int(a[3]), int(a[4]), float(a[5]),
                 a[6] if len(a) > 6 else None)


if __name__ == "__main__":
    main()
