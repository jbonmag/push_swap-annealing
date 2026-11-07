# push_swap-annealing

**English** · [Español](README.es.md)

A solution to 42's *push_swap* that treats the exercise as an optimization problem: instead of a fixed sorting recipe, it searches for a short sequence of operations with simulated annealing.

| | n = 100 | n = 500 |
|---|---|---|
| **This program** | **400.4** (372–425) | **3274.9** (3145–3452) |
| 42's limit for full marks | 700 | 5500 |

Averages over the same 20 seeded random inputs per size, with every output checked by an independent checker. It takes about 0.9 s per run at n = 100 and 1.6 s at n = 500. Searching longer gives shorter solutions, down to 382.3 / 3119.4 in about 20 s: see [Speed versus length](#speed-versus-length).

---

## Contents

1. [The exercise](#the-exercise)
2. [The challenge we set ourselves](#the-challenge-we-set-ourselves)
3. [How it works](#how-it-works)
4. [Simulated annealing in depth](#simulated-annealing-in-depth)
5. [Speed versus length](#speed-versus-length)
6. [How we got here](#how-we-got-here)
7. [What did not work](#what-did-not-work)
8. [How far from optimal?](#how-far-from-optimal)
9. [Rules and compliance](#rules-and-compliance)
10. [Build, run and test](#build-run-and-test)
11. [Repository layout](#repository-layout)
12. [License](#license)

---

## The exercise

*push_swap* is a project of the 42 school curriculum. It is summarised here in our own words.

You get a list of distinct integers on a stack **a**, and an empty stack **b**. You must print a list of operations that leaves **a** sorted in ascending order, smallest on top, with **b** empty. There are eleven operations:

| Operation | Effect |
|---|---|
| `sa`, `sb`, `ss` | Swap the top two elements of a, of b, or of both |
| `pa`, `pb` | Move the top element of b onto a, or of a onto b |
| `ra`, `rb`, `rr` | Rotate up: the top element goes to the bottom (a, b, or both) |
| `rra`, `rrb`, `rrr` | Rotate down: the bottom element goes to the top (a, b, or both) |

The program must print the **shortest list it can**. Evaluators run it on random inputs of 100 and 500 numbers. Full marks need fewer than 700 operations for 100 numbers and no more than 5500 for 500. The code must be in C, follow 42's style guide (the *Norm*), use only `read`, `write`, `malloc`, `free` and `exit`, have no global variables, never crash or leak, and print `Error` on bad input.

The usual approach is a direct algorithm, such as chunk or bucket sorting, cheapest-move insertion, or radix sort. Such algorithms run in milliseconds.

## The challenge we set ourselves

This repository is not a school submission. It is a personal attempt to find out **how few operations are really possible**, within the subject's rules:

- **Targets:** under 350 operations for 100 numbers and under 2500 for 500. Neither has been reached; the best figures so far are 382 and 3119, with a 20 s budget.
- **Rules:** everything the subject requires. That means one thread (no `pthread`, `fork` or clock), no math library, and the Norm.
- **Time:** the subject sets no time limit. We chose speed: about a second per run by default, with longer searches one line away ([Speed versus length](#speed-versus-length)).
- **Method:** every experiment, including every failure, is logged with its numbers in [`research/BITACORA.md`](research/BITACORA.md) (in Spanish). That log has 43 numbered experiments and several diagnostics.

## How it works

### 1. A solution is four decisions

Every solution this program produces has two phases. In the **push phase**, some elements move from a to b. In the **pull phase**, they all come back, each into its sorted place. Such a solution is fully described by four choices:

| Part | Meaning |
|---|---|
| **keep** | Elements that never leave a. They must already be in cyclically increasing order. |
| **sigma** | The order in which the other elements leave a. |
| **pi** | The circular order they form in b. |
| **tau** | The order in which they come back to a. |

Given these four, the operations follow mechanically. Each move takes one element to its place in the other stack, rotating both stacks with the cheapest of four combinations: `rr` together, `rrr` together, or opposite directions. Exact search on small inputs shows that this two-phase family loses almost nothing: at n = 10, forcing all pushes before all pulls costs 0.04 operations on average.

### 2. Simulated annealing over all four at once

**Where the name comes from.** *Annealing* is a heat treatment for metal and glass: the material is heated and then cooled very slowly, so its atoms have time to settle into an ordered, low-energy structure. Cooled quickly, they get trapped in a disordered, brittle one. *Simulated annealing* is an optimisation method from 1983 that imitates this process.

**How it applies here.** The program repeatedly makes a small random change and keeps it if it shortens the solution. Sometimes it also keeps a change that makes the solution longer. How readily it accepts a worse solution is set by the "temperature". It starts high, so the search explores freely, and falls slowly until only improvements are accepted. That lets the search escape local minima instead of stopping at the first one it finds. In the code, `SA_T0` and `SA_T1` are the starting and final temperatures, and "reheating" means raising it again. The exact rule, with its formula, is in [Simulated annealing in depth](#simulated-annealing-in-depth).

The moves include:

- move one element in sigma or tau;
- move a block of elements;
- move an element into or out of keep;
- reorder pi;
- compound moves, where an element moves next to the same neighbour in pi and in one of the two orders.

### 3. The starting shape of b matters most

Annealing can polish a solution, but it cannot reorganise stack b on a large scale. The start decides much of the result: on one 500-number input, different starts gave anything from 3391 to 5332. So the program does not trust a single start:

- **160 "ring" shapes compete.** Each is built around one rank. Sweep after sweep, a gives up the ranks further and further from it, the upper ones to one end of b and the lower ones to the other. The shapes differ in centre, direction, number of sweeps (1 to 10) and how the sweep widths grow.
- **Successive halving.** All 160 run briefly, the worse half is dropped, the survivors get more work, and so on. This takes 30% of the budget.
- **Polishing.** The winner is reheated and gets the remaining 70%.

### 4. Fast evaluation

The search spends almost all its time asking "how long is this solution?". Two tricks make that cheap:

- **Bitsets.** Each step needs to count elements between two positions, which takes a few word operations.
- **Checkpoints.** A move only changes part of the sequence, so the evaluator resumes from a saved state just before the change and stops as soon as the move is clearly lost.

Together they took the search from about 600 to over 800,000 evaluations per second at n = 500.

### 5. Final touches

- **Zigzags into swaps.** A last pass turns patterns like `ra pb rra pb` into `sa pb pb`.
- **Small inputs:** 2 and 3 elements have dedicated code, and 5 elements never need more than 10 operations.
- **Very large inputs** of more than 512 numbers fall back to radix sort.

### 6. Deterministic by design

The search stops after a **fixed amount of work** (`SA_BUDGET` in `src/sa.h`), not after a fixed time. The same input always gives the same output, on any machine. A faster machine finishes sooner; a slower one takes longer, but prints exactly the same answer.

## Simulated annealing in depth

This section is for readers who want the mathematics. It follows the method of Kirkpatrick, Gelatt and Vecchi (*Science*, 1983), which builds on the Metropolis rule from statistical physics (1953).

### The energy

Annealing minimises a quantity called the **energy**. Here the energy of a solution $s$ is simply its length: the number of operations it prints.

```math
E(s) = \text{number of operations of } s
```

A **move** turns $s$ into a neighbouring solution $s'$, and its cost is the change in energy:

```math
\Delta = E(s') - E(s)
```

$\Delta < 0$ means the move shortens the solution, $\Delta = 0$ leaves its length unchanged, and $\Delta > 0$ lengthens it. Both are counted in operations, so the temperature below is also measured in operations.

### The acceptance rule (Metropolis)

At temperature $T$, a move is accepted with probability

```math
P(\text{accept}) =
\begin{cases}
1 & \text{if } \Delta \le 0 \\
e^{-\Delta / T} & \text{if } \Delta > 0
\end{cases}
```

Improvements are always kept. A worsening move is kept sometimes: small worsenings often, large ones rarely, and less and less often as $T$ falls. This is what separates annealing from plain hill climbing, which would freeze in the first local minimum it reaches.

The values this program actually uses give an idea of the scale:

| Move makes the solution longer by | at $T = 1.2$ (start) | at $T = 0.6$ (reheated winner) | at $T = 0.15$ (end) |
|---|---|---|---|
| 1 operation | 43% | 19% | 0.13% |
| 2 operations | 19% | 3.6% | 0.0002% |
| 3 operations | 8.2% | 0.67% | ≈ 0 |
| 5 operations | 1.6% | 0.02% | ≈ 0 |

At the start, the search happily accepts a loss of one or two operations to escape a bad shape. At the end, it is practically a pure descent.

### Why this works: the Boltzmann distribution

If the temperature were held fixed and the search ran for long enough, it would visit each solution $s$ with probability proportional to

```math
\pi_T(s) \propto e^{-E(s)/T}
```

This is the Boltzmann distribution of statistical physics. At high $T$ it is nearly flat, so the search wanders widely. As $T \to 0$, all its weight concentrates on the solutions of lowest energy, which are the shortest ones. Cooling slowly lets the search follow that distribution as it narrows, instead of falling into whichever local minimum is nearest.

There is a theoretical guarantee (Hajek, 1988): with a logarithmic schedule $T_k = c / \ln k$, where $c$ is large enough, the search reaches a global minimum with probability tending to 1. That schedule is far too slow to be practical, so real programs, this one included, cool faster and accept that the guarantee is lost.

### The cooling schedule

This program cools **geometrically**: the temperature falls by the same factor for each equal share of the work.

```math
T(f) = T_0 \left(\frac{T_1}{T_0}\right)^{f} = T_0^{\,1-f}\; T_1^{\,f}, \qquad 0 \le f \le 1
```

$f$ is the fraction of the work done so far. The temperature is updated every 1024 trials. Two coolings run one after the other:

| Stage | Share of the work | From | To | Constants in `src/sa.h` |
|---|---|---|---|---|
| Race of the 160 starting shapes (9 rounds of successive halving) | 30% | 1.2 | 0.15 | `SA_T0`, `SA_T1` |
| Polishing the winner | 70% | 0.6 | 0.15 | `SA_TR`, `SA_T1` |

During the race, round $r$ of $R = 9$ covers the stretch of the schedule from $f = r/R$ to $f = (r+1)/R$. So the race as a whole is a single cooling, shared by every surviving shape. The winner is then reheated to 0.6, which is hot enough to reorganise it but not to destroy it, and cooled again with the remaining 70%.

### How the code applies the rule

The Metropolis test can be rewritten so that the random number is drawn **before** the move is evaluated. With $u$ uniform in $(0, 1)$:

```math
u < e^{-\Delta/T} \iff \Delta < T\,(-\ln u)
```

So each trial first draws a threshold $d = T\,(-\ln u)$, and then accepts the move if $\Delta \le d$. Since $\Delta$ is a whole number, comparing it with $\lfloor d \rfloor$ gives the same answer. In `src/sa_anneal.c` and `src/sa_span.c` this reads, in simplified form:

```c
d = (int)(temp * logu[rnd() % 4096]);  /* the threshold T * (-ln u) */
/* ... evaluate the move, giving delta ... */
accepted = (delta <= d);
```

Drawing the threshold first has two advantages:

- **No logarithm or exponential in the inner loop.** The values of $-\ln u$ are precomputed once in a table of 4096 entries (`logu`). The subject allows no math library, so `sa_ln` and `sa_exp` are written by hand, and they only run when the table is built and when the temperature changes.
- **Early abandon.** Because the trial knows the largest $\Delta$ it could accept, the evaluator can stop simulating as soon as the move is clearly lost. That is when the partial cost exceeds $d$ plus a small margin (`SA_SLACK` = 8 operations), so the full length never has to be computed. Most moves are rejected, so this saves a large share of the work.

The random numbers come from a 64-bit xorshift generator with a fixed seed. That is why the same input always gives the same output.

### The moves

Each trial picks one kind of move at random. These are the proportions in `src/sa_anneal.c`:

| Move | Share of trials |
|---|---|
| Reorder sigma or tau (half each): move a block of 1–3 elements elsewhere (88%) or reverse a stretch (12%) | 91% |
| Reorder pi, the circular order in b | 4% |
| Compound move: an element moves next to the same neighbour in pi and in one order | 4% |
| Move an element into or out of keep | 1% |

Moves that change only the order of the push or pull phase are cheap to evaluate, because only the affected stretch of the solution is re-simulated. That is why they make up almost all of the trials.

## Speed versus length

The program is tuned to answer in about a second. Searching longer gives shorter solutions. This section shows how much shorter, how far that goes, and why we chose speed.

**What "more time" means.** The search makes small random changes and keeps the good ones (see [How it works](#how-it-works)). `SA_BUDGET` in `src/sa.h` sets how much of that work it does, so doubling the budget roughly doubles the time. The research engine tries about 1.8 million changes per second at n = 100 and 0.6 million at n = 500.

The same program with different budgets, on the same 20 inputs:

| `SA_BUDGET` | Time alone, n = 100 / 500 | n = 100 | n = 500 |
|---|---|---|---|
| **4·10⁷ (default)** | **0.9 s / 1.6 s** | **400.4** | **3274.9** |
| 8·10⁷ | 1.7 s / 2.6 s | 396.9 | 3206.1 |
| 2·10⁸ | 4.4 s / 5.7 s | 392.0 | 3164.4 |
| 9·10⁸ | 20 s / 23 s | 382.3 | 3119.4 |

**How far it keeps improving:**

- **n = 500 gains most at first.** Going from 1.6 s to 5.7 s saves 110 operations (3.4%). Going on to 23 s saves another 45 (1.4%).
- **n = 100 improves slowly but steadily,** by about 13 operations each time the run is ten times longer. An earlier version of the research engine, without the ring shapes, was still improving after 30 minutes per input, at an average of 371.4.
- **So there is no sharp end point,** only shrinking returns. Beyond about 20–30 s, each tenfold increase in time buys a few percent at most, and runs of half an hour are not practical.

**Why the gains shrink.** Annealing settles the local order within the first second: exact search over windows of 8–12 elements finds nothing left to improve. What remains is the large-scale structure, meaning the shape of b and which elements stay in a. That only changes through rare, coordinated moves, so each further improvement takes longer to find. It is also why a much better result would need a new idea, not more time (see [What did not work](#what-did-not-work)).

**Why we chose speed.** At about a second, the program answers as quickly as people expect, and it already gets 400 / 3275, far inside 42's limits of 700 / 5500. Longer searches buy the last 5%. Anyone who wants the shortest output can raise `SA_BUDGET`. Each value always gives the same output for the same input.

The program uses a single core, because the subject does not allow threads or processes.

## How we got here

Each step was measured on seeded, paired inputs and verified by a checker. Figures are average operations for n = 100 / n = 500.

| Step | n = 100 | n = 500 | What changed |
|---|---|---|---|
| Starting point (heuristics + beam search on the pull order) | 456.8 | 3924.8 | Earlier version |
| Whole-solution simulated annealing, first prototype | 406.8 (600 s) | — | Showed that the supposed "floor" of 420–440 was not real |
| Bitset evaluation with checkpoints | 400 (60 s) | 3427 (60 s) | About 600 times more evaluations per second |
| Competing starting shapes for b | 392 (60 s) | 3302 (60 s) | The shape of b matters more than any fine tuning |
| Tuning on 20 paired inputs | 383.8 (60 s) | 3235.5 (60 s) | Most of the work goes to polishing the winner |
| Port to C under the Norm, fixed work budget | 397 (~4 s) | 3309 (~5 s) | First version inside push_swap |
| Ring shapes with wider first sweeps | 382.3 (~20 s) | 3119.4 (~23 s) | The biggest single gain at n = 500 |
| Default budget lowered to about 1 s | **400.4** (~0.9 s) | **3274.9** (~1.6 s) | About 20 times faster for 4.7% / 5.0% more operations |

## What did not work

These are the most useful part of [`research/BITACORA.md`](research/BITACORA.md). Each idea was tested against the current program on the same inputs.

| Idea | Result | Why it failed |
|---|---|---|
| Fixed quicksort-style passes before annealing | 499 / 3892 | Each pass costs about twice what it saves |
| Annealing two passes with an intermediate order | 550 / 4207 | Same problem, even when optimised |
| Superoptimising windows of the output | −3 at n = 100 | The annealer leaves almost no local slack |
| Inverted pairs in a, fixed with `sa` | +4 / +13 | Promised savings vanished once rotations were counted |
| Solving the inverse permutation | Within noise | Changes the start, not the result |
| Ruin and recreate (remove 2–30 elements, reinsert) | Reaches the family optimum at n = 10, nothing at n = 100 / 500 | Annealing already finds the same moves |
| Exact dynamic programming over windows of 8–12 | 0 gain | The solution is already locally optimal; what is missing is structure |
| Multiscale (solve a 100 sample, lift to 500) | Worse starts than rings | — |
| Four transfers (leave some elements unsorted in a for a second round) | 3889–4715 vs 3080 | Waiting elements are obstacles to every other move |
| Restarts: best of many short runs | Loses to one long run | The race of shapes already exploits that variation |
| **Second trips** (park an element in the other stack, or send it out again) | 0–4 gained on ~2000 | Each extra trip costs 2 and almost never saves 2 rotations |
| **A learned heuristic** (transformer trained on GPU, guided beam search) | Optimal at n = 10; at n = 100 solves 0 of 5 inputs after 6 rounds of DAgger | Search falls into states the network wrongly thinks are almost solved; also far too slow for C in 30 s |

## How far from optimal?

An exact breadth-first search over every state gives the true optimum for small n:

| n | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|
| Average optimum | 1.17 | 2.88 | 4.86 | 7.06 | 9.31 | 11.59 | 13.91 | 16.28 |

At n = 10 this program is about 2.5 operations above the optimum. 1.7 of those come from swaps, which optimal solutions use to fix pairs on the way. For large n no exact answer is known. Counting how fast the number of reachable states grows gives a rough lower bound of about 237 operations for n = 100 and 1700 for n = 500. That is not a proof, so the real optimum may be well above it.

Another way to see it is information per operation. Sorting n numbers needs log2(n!) bits: 525 for 100 and 3768 for 500.

| | n = 100 | n = 500 |
|---|---|---|
| This program, default budget (~1 s) | 1.31 bits/op | 1.15 |
| This program, 20 s budget | 1.37 | 1.21 |
| Needed for 350 / 2500 | 1.50 | 1.51 |
| Optimum at n = 10 | 1.34 | — |

## Rules and compliance

| Rule | Status |
|---|---|
| Only `read`, `write`, `malloc`, `free`, `exit` | Yes. Popcount and count-trailing-zeros are written by hand; there is no math library |
| One thread, no clock | Yes. It stops after a fixed amount of work |
| No global variables | Yes, in `src/` |
| Norm | `norminette` reports only the missing 42 file headers. They are personal to each student and were left out because this is not a submission |
| No crashes or leaks | Valgrind is clean at n = 30 and n = 700, and on error paths |
| `Error` on bad input; nothing for no arguments | Yes |
| Makefile with `cc`, `-Wall -Wextra -Werror`, no relink | Yes |

`research/` and `tools/` are test programs. They do not follow the Norm and are not part of the build.

## Build, run and test

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

## Repository layout

| Path | Contents |
|---|---|
| `src/` | The program. `sa_*.c` is the annealer, `sa_arms.c` and `sa_seed.c` build the ring shapes, and `sa_race.c` runs successive halving |
| `tools/bench.py` | Seeded benchmark with an independent checker |
| `research/engine.c` | Research engine: the same search, plus every experimental switch from the lab notebook |
| `research/bfs_exact.c` | Exact optimum for n ≤ 10 |
| `research/park.c` | Second-trip probe |
| `research/s9_*.py` | Learned-heuristic experiments (PyTorch) |
| `research/BITACORA.md` | The full lab notebook, in Spanish: rules, measurements, every experiment and every mistake |

## License

The code and documentation are released under the [MIT License](LICENSE). The subject of the exercise belongs to 42 and is not included here.
