# push_swap-annealing

**English** · [Español](README.es.md)

> *Most push_swap solutions follow a recipe. This one is forged.*

A solution to 42's *push_swap* that treats sorting like smithing. Instead of following a fixed procedure, it heats a rough solution, hammers it thousands of times a second, and lets it cool slowly until the blade holds its shape. The technique is called **simulated annealing**, and it is named after exactly that: what a blacksmith does to steel.

| | n = 100 | n = 500 |
|---|---|---|
| **This blade** | **400.4** (372–425) | **3274.9** (3145–3452) |
| 42's limit for full marks | 700 | 5500 |

These are averages over the same 20 seeded random inputs per size, and an independent checker verifies every output. A blade takes about 0.9 s to forge at n = 100 and 1.6 s at n = 500. Leave it longer at the anvil and the edge gets finer, down to 382.3 / 3119.4 in about 20 s: see [How long at the anvil](#how-long-at-the-anvil).

---

## Contents

1. [The ore: the exercise](#the-ore-the-exercise)
2. [The commission: our challenge](#the-commission-our-challenge)
3. [The forge: how it works](#the-forge-how-it-works)
4. [How long at the anvil: speed versus length](#how-long-at-the-anvil)
5. [From scrap iron to steel: how we got here](#from-scrap-iron-to-steel-how-we-got-here)
6. [The scrap heap: what did not work](#the-scrap-heap-what-did-not-work)
7. [The perfect blade: how far from optimal?](#the-perfect-blade-how-far-from-optimal)
8. [The guild's rules: compliance](#the-guilds-rules-compliance)
9. [Test the edge: build, run and test](#test-the-edge-build-run-and-test)
10. [The workshop: repository layout](#the-workshop-repository-layout)
11. [License](#license)

---

## The ore: the exercise

*push_swap* is a project of the 42 school curriculum. It is summarised here in our own words.

You get a list of distinct integers on a stack **a**, and an empty stack **b**. You must print a list of operations that leaves **a** sorted in ascending order, smallest on top, with **b** empty. There are eleven operations, the only tools on the bench:

| Operation | Effect |
|---|---|
| `sa`, `sb`, `ss` | Swap the top two elements of a, of b, or of both |
| `pa`, `pb` | Move the top element of b onto a, or of a onto b |
| `ra`, `rb`, `rr` | Rotate up: the top element goes to the bottom (a, b, or both) |
| `rra`, `rrb`, `rrr` | Rotate down: the bottom element goes to the top (a, b, or both) |

The program must print the **shortest list it can**. Evaluators run it on random inputs of 100 and 500 numbers. Full marks need fewer than 700 operations for 100 numbers and no more than 5500 for 500. The code must be in C, follow 42's style guide (the *Norm*), use only `read`, `write`, `malloc`, `free` and `exit`, have no global variables, never crash or leak, and print `Error` on bad input.

The usual approach is a direct algorithm, such as chunk or bucket sorting, cheapest-move insertion, or radix sort. That is casting: pour the metal into a mould once, and the shape is whatever the mould gives. It is fast, and it is fixed.

## The commission: our challenge

This repository is not a school submission. It is a personal attempt to find out **how few operations are really possible**, within the subject's rules:

- **Targets:** under 350 operations for 100 numbers and under 2500 for 500. Neither has been reached yet; the best figures so far are 382 and 3119, with a 20 s budget.
- **Rules:** everything the subject requires. That means one thread (no `pthread`, `fork` or clock), no math library, and the Norm.
- **Time:** the subject sets no time limit. We chose speed: about a second per blade by default, with longer forging one line away ([How long at the anvil](#how-long-at-the-anvil)).
- **Method:** every experiment, including every blade that broke, is logged with its numbers in [`research/BITACORA.md`](research/BITACORA.md), the smith's notebook (in Spanish). It holds 43 numbered experiments and several diagnostics.

## The forge: how it works

### 1. The anatomy of a blade: four decisions

Every solution this program forges has two phases. In the **push phase**, some elements move from a to b. In the **pull phase**, they all come back, each into its sorted place. A solution is fully described by four choices:

| Part | Meaning |
|---|---|
| **keep** | Elements that never leave a. They must already be in cyclically increasing order. |
| **sigma** | The order in which the other elements leave a. |
| **pi** | The circular order they form in b. |
| **tau** | The order in which they come back to a. |

Given these four, the operations follow mechanically. Each move takes one element to its place in the other stack, rotating both stacks with the cheapest of four combinations: `rr` together, `rrr` together, or opposite directions. Exact search on small inputs shows that this two-phase design loses almost nothing: at n = 10, forcing all pushes before all pulls costs 0.04 operations on average.

### 2. Heat and slow cooling: simulated annealing

**What annealing is.** To *anneal* steel, a smith heats it and lets it cool very slowly. The slow cooling gives the atoms time to settle into an ordered, low-stress structure; cooled too fast, they freeze in a disordered, brittle one. (It is the gentle cousin of *quenching*, the sudden plunge into water that hardens a blade. Our algorithm only anneals.) *Simulated annealing*, from 1983, does the same with solutions instead of atoms.

**At our anvil.** The program strikes the solution again and again: each blow is a small random change, kept if it shortens the solution. While the metal is hot, it also accepts some blows that make the solution longer. That is what lets it escape a bad shape instead of hardening into the first one it finds. The chance of accepting such a blow is the **temperature**. It starts high, falls slowly, and at the end only improvements get through. In the code, `SA_T0` and `SA_T1` are the starting and final temperatures, and "reheating" means putting the metal back in the fire.

The hammer blows include:

- move one element in sigma or tau;
- move a block of elements;
- move an element into or out of keep;
- reorder pi;
- compound blows, where an element moves next to the same neighbour in pi and in one of the two orders.

### 3. 160 billets in the fire

Hammering can refine a blade, but it cannot turn a dagger into a sabre: annealing cannot reorganise stack b on a large scale. The starting bar decides much of the result. On one 500-number input, different starts gave anything from 3391 to 5332. So the smith never trusts a single bar:

- **160 billets go into the fire.** Each is a "ring" shape built around one rank. Sweep after sweep, a gives up the ranks further and further from it, the upper ones to one end of b and the lower ones to the other. The billets differ in centre, direction, number of sweeps (1 to 10) and how the sweep widths grow.
- **The worst half goes to the scrap bin, round after round.** All 160 are hammered briefly, the worse half is discarded, and the survivors get more hammering. This is successive halving, and it uses 30% of the work.
- **The chosen billet goes back into the fire.** The winner is reheated and gets the remaining 70% of the work.

### 4. A smith's eye: fast evaluation

Between blows, the smith must judge the blade: "how long is this solution?" That question is asked hundreds of thousands of times a second, so it has to be cheap:

- **Bitsets.** Each step needs to count elements between two positions, which takes a few word operations.
- **Checkpoints.** A blow only changes part of the blade, so the evaluator resumes from a saved state just before the change. It also stops as soon as the blow is clearly a bad one.

Together they took the forge from about 600 to over 800,000 judgements per second at n = 500.

### 5. Grinding and polishing

- **Zigzags into swaps.** A last pass turns patterns like `ra pb rra pb` into `sa pb pb`.
- **Tiny inputs:** 2 and 3 elements have dedicated code, and 5 elements never need more than 10 operations.
- **Huge inputs** of more than 512 numbers fall back to radix sort. Some bars are too big for this forge.

### 6. Same steel, same sword

The forge stops after a **fixed amount of work** (`SA_BUDGET` in `src/sa.h`), not after a fixed time. The same input always gives the same output, on any machine. A faster machine finishes sooner; a slower one takes longer, but forges exactly the same blade.

## How long at the anvil

*Speed versus length.* The forge is tuned to deliver a blade in about a second. More hours at the anvil give a finer edge. This section shows how much finer, how far that goes, and why we chose speed.

**What "more time" means.** More time means more hammer blows. `SA_BUDGET` sets how much work the forge does, so doubling it roughly doubles the time. The research engine tries about 1.8 million blows per second at n = 100 and 0.6 million at n = 500.

The same forge with different budgets, on the same 20 inputs:

| `SA_BUDGET` | Time alone, n = 100 / 500 | n = 100 | n = 500 |
|---|---|---|---|
| **4·10⁷ (default)** | **0.9 s / 1.6 s** | **400.4** | **3274.9** |
| 8·10⁷ | 1.7 s / 2.6 s | 396.9 | 3206.1 |
| 2·10⁸ | 4.4 s / 5.7 s | 392.0 | 3164.4 |
| 9·10⁸ | 20 s / 23 s | 382.3 | 3119.4 |

**How far it keeps improving:**

- **n = 500 gains most at first.** Going from 1.6 s to 5.7 s saves 110 operations (3.4%). Going on to 23 s saves another 45 (1.4%).
- **n = 100 improves slowly but steadily,** by about 13 operations each time the run is ten times longer. An earlier version of the research engine, without the ring billets, was still improving after 30 minutes per input, at an average of 371.4.
- **So there is no sharp end point,** only shrinking returns. Beyond about 20–30 s, each tenfold increase in time buys a few percent at most, and half an hour at the anvil is not practical.

**Why the gains shrink.** The fine detail of the edge is settled within the first second: exact search over windows of 8–12 elements finds nothing left to improve. What remains is the shape of the blade as a whole, meaning the shape of b and which elements stay in a. That only changes through rare, coordinated blows, so each further improvement takes longer to find. It is also why a much better blade would need a new technique, not more hammering (see [The scrap heap](#the-scrap-heap-what-did-not-work)).

**Why we chose speed.** In about a second, the program answers as quickly as people expect, and it already gets 400 / 3275, far inside 42's limits of 700 / 5500. Long forging buys the last 5%. Anyone who wants the finest edge can raise `SA_BUDGET`. Each value always forges the same blade for the same input.

The forge has one smith: the subject does not allow threads or processes, so the program runs on one core.

## From scrap iron to steel: how we got here

Each step was measured on seeded, paired inputs and verified by a checker. Figures are average operations for n = 100 / n = 500.

| Step | n = 100 | n = 500 | What changed |
|---|---|---|---|
| Starting point (heuristics + beam search on the pull order) | 456.8 | 3924.8 | Earlier version |
| Whole-solution simulated annealing, first prototype | 406.8 (600 s) | — | Showed that the supposed "floor" of 420–440 was not real |
| Bitset evaluation with checkpoints | 400 (60 s) | 3427 (60 s) | About 600 times more judgements per second |
| Competing starting billets | 392 (60 s) | 3302 (60 s) | The shape of b matters more than any fine tuning |
| Tuning on 20 paired inputs | 383.8 (60 s) | 3235.5 (60 s) | Most of the work goes to the chosen billet |
| Port to C under the Norm, fixed work budget | 397 (~4 s) | 3309 (~5 s) | First blade out of the real forge |
| Ring billets with wider first sweeps | 382.3 (~20 s) | 3119.4 (~23 s) | The biggest single gain at n = 500 |
| Default budget lowered to about 1 s | **400.4** (~0.9 s) | **3274.9** (~1.6 s) | About 20 times faster for 4.7% / 5.0% more operations |

## The scrap heap: what did not work

Every smith has a pile of blades that cracked. Ours is the most useful part of [`research/BITACORA.md`](research/BITACORA.md). Each idea was tested against the current forge on the same inputs.

| Idea | Result | Why it cracked |
|---|---|---|
| Fixed quicksort-style passes before annealing | 499 / 3892 | Each pass costs about twice what it saves |
| Annealing two passes with an intermediate order | 550 / 4207 | Same problem, even when optimised |
| Superoptimising windows of the output | −3 at n = 100 | The annealer leaves almost no local slack |
| Inverted pairs in a, fixed with `sa` | +4 / +13 | Promised savings vanished once rotations were counted |
| Solving the inverse permutation | Within noise | Changes the starting bar, not the blade |
| Ruin and recreate (remove 2–30 elements, reinsert) | Reaches the family optimum at n = 10, nothing at n = 100 / 500 | Annealing already finds the same moves |
| Exact dynamic programming over windows of 8–12 | 0 gain | The edge is already locally perfect; what is missing is structure |
| Multiscale (solve a 100 sample, lift to 500) | Worse starts than rings | — |
| Four transfers (leave some elements unsorted in a for a second round) | 3889–4715 vs 3080 | Waiting elements get in the way of every other blow |
| Restarts: best of many short runs | Loses to one long run | The 160 billets already exploit that variation |
| **Second trips** (park an element in the other stack, or send it out again) | 0–4 gained on ~2000 | Each extra trip costs 2 and almost never saves 2 rotations |
| **A learned heuristic** (transformer trained on GPU, guided beam search) | Optimal at n = 10; at n = 100 solves 0 of 5 inputs after 6 rounds of DAgger | The search falls into states the network wrongly believes are almost finished; also far too slow for C |

## The perfect blade: how far from optimal?

The perfect blade exists, but nobody knows its shape for large n. For small n, an exact breadth-first search over every state finds it:

| n | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|
| Average optimum | 1.17 | 2.88 | 4.86 | 7.06 | 9.31 | 11.59 | 13.91 | 16.28 |

At n = 10 this forge is about 2.5 operations above the optimum. 1.7 of those come from swaps, which optimal solutions use to fix pairs on the way. For large n, counting how fast the number of reachable states grows gives a rough lower bound of about 237 operations for n = 100 and 1700 for n = 500. That is not a proof, so the real optimum may be well above it.

Another way to see it is information per operation. Sorting n numbers needs log2(n!) bits: 525 for 100 and 3768 for 500.

| | n = 100 | n = 500 |
|---|---|---|
| This forge, default budget (~1 s) | 1.31 bits/op | 1.15 |
| This forge, 20 s budget | 1.37 | 1.21 |
| Needed for 350 / 2500 | 1.50 | 1.51 |
| Optimum at n = 10 | 1.34 | — |

## The guild's rules: compliance

| Rule | Status |
|---|---|
| Only `read`, `write`, `malloc`, `free`, `exit` | Yes. Popcount and count-trailing-zeros are hand-forged; there is no math library |
| One thread, no clock | Yes. It stops after a fixed amount of work |
| No global variables | Yes, in `src/` |
| Norm | `norminette` reports only the missing 42 file headers. They are personal to each student and were left out because this is not a submission |
| No crashes or leaks | Valgrind is clean at n = 30 and n = 700, and on error paths |
| `Error` on bad input; nothing for no arguments | Yes |
| Makefile with `cc`, `-Wall -Wextra -Werror`, no relink | Yes |

`research/` and `tools/` are the workshop's test rigs. They do not follow the Norm and are not part of the build.

## Test the edge: build, run and test

```bash
make
./push_swap 3 1 2 5 4
ARG=$(python3 -c "import random; print(*random.sample(range(100000), 100))")
./push_swap $ARG | wc -l
./push_swap $ARG | ./checker_linux $ARG
```

`tools/bench.py` runs any solver on seeded inputs and verifies every output itself:

```bash
python3 tools/bench.py run ./push_swap 100 20 --seed 8
python3 tools/bench.py exhaustive ./push_swap 5
python3 tools/bench.py compare ./other_push_swap ./push_swap 500 20
```

The figures in this README use `--seed 8`.

## The workshop: repository layout

| Path | Contents |
|---|---|
| `src/` | The forge itself. `sa_*.c` is the annealer, `sa_arms.c` and `sa_seed.c` cast the ring billets, and `sa_race.c` sends the worse half to the scrap bin |
| `tools/bench.py` | Seeded benchmark with an independent checker |
| `research/engine.c` | The experimental forge: the same search, plus every switch from the notebook |
| `research/bfs_exact.c` | Exact optimum for n ≤ 10: the perfect blade, where it can be found |
| `research/park.c` | Second-trip probe |
| `research/s9_*.py` | Learned-heuristic experiments (PyTorch) |
| `research/BITACORA.md` | The smith's notebook, in Spanish: rules, measurements, every experiment and every mistake |

## License

The code and documentation are released under the [MIT License](LICENSE). The subject of the exercise belongs to 42 and is not included here.
