/*
** Research solver, not part of the graded build and not under the Norm.
** Simulated annealing over a whole solution of the family "one transfer a->b,
** one transfer b->a". The genome has four parts:
**   keep  elements that never leave a (must stay cyclically sorted)
**   sigma order in which elements leave a
**   pi    circular order the elements must have in b
**   tau   order in which elements return to a
** Both transfers are the same operation: move x from a source circular list
** to its coordinate in a destination circular list, with the cheapest of the
** four rotation combos. Popcounts make a step O(words), and a local reorder
** only re-simulates the affected range, from a checkpoint.
**
** usage: engine SECONDS SEED num...     (ops on stdout, stats on stderr)
** build: gcc -O2 -march=native -DW=2 research/engine.c -lm    (n <= 128)
**        gcc -O2 -march=native research/engine.c -lm          (n <= 512)
*/
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef W
# define W 8
#endif
#define MX (W * 64)
#define MXI (MX + 64)

typedef uint64_t	t_u64;

typedef struct s_state
{
	t_u64	src[W];
	t_u64	dst[W];
	int		stop;
	int		dtop;
	int		ssz;
	int		dsz;
	int		zz;
}	t_state;

typedef struct s_gene
{
	int	keep[MX];
	int	sigma[MXI];
	int	pi[MX];
	int	tau[MX];
	int	m;
	int	ms;
	int	defer[MX];
	int	sig2[MX];
	int	pi2[MX];
	int	tau2[MX];
	int	m2;
}	t_gene;

static int				g_n;
static int				g_init[MX];
static int				g_ipos[MX];
static t_gene			g;
static int				g_pipos[MX];
static int				g_spos[MXI];
static int				g_atc[MX];
static int				g_pu[64];
static int				g_pv[64];
static int				g_npairs;
static char				g_inpair[MX];
static int				g_tpos[MX];
static t_state			g_ck0[MXI + 1];
static t_state			g_ck1[MX + 1];
static int				g_pre0[MXI + 1];
static int				g_pre1[MX + 1];
static int				g_finish;
static t_u64			g_rng;
static FILE				*g_out;

/* FOUR=1: four transfers. The deferred elements stay in a, out of order, while
** the others leave and return; then they leave (T3) and return (T4) on their
** own. During T2 a is not in rank order, so its coordinates come from g_ca */
static int				g_four = 0;
static int				g_ca[MX];
static int				g_ac[MX];
static int				g_s2pos[MX];
static int				g_p2pos[MX];
static int				g_t2pos[MX];

static inline int	ac(int x)
{
	return (g_four ? g_ca[x] : x);
}

static inline unsigned int	rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return ((unsigned int)(g_rng >> 20));
}

static inline int	count_range(const t_u64 *b, int lo, int hi)
{
	int	wl = lo >> 6, wh = hi >> 6, c = 0, i;

	if (lo >= hi)
		return (0);
	if (wl == wh)
		return (__builtin_popcountll(b[wl]
				& (((hi & 63) ? (1ULL << (hi & 63)) : 0) - (1ULL << (lo & 63)))));
	c = __builtin_popcountll(b[wl] >> (lo & 63));
	for (i = wl + 1; i < wh; i++)
		c += __builtin_popcountll(b[i]);
	if (hi & 63)
		c += __builtin_popcountll(b[wh] & ((1ULL << (hi & 63)) - 1));
	return (c);
}

/* set bits met when walking cyclically from `from` up to `to`, excluding `to` */
static inline int	cyc(const t_u64 *b, int from, int to, int dom)
{
	if (from <= to)
		return (count_range(b, from, to));
	return (count_range(b, from, dom) + count_range(b, 0, to));
}

static inline int	next_set(const t_u64 *b, int c, int dom)
{
	int		w = c >> 6, i;
	t_u64	v = b[w] & (~0ULL << (c & 63));

	if (v && (w << 6) + __builtin_ctzll(v) < dom)
		return ((w << 6) + __builtin_ctzll(v));
	for (i = w + 1; i <= (dom - 1) >> 6; i++)
		if (b[i])
			return ((i << 6) + __builtin_ctzll(b[i]));
	for (i = 0; i <= w; i++)
		if (b[i])
			return ((i << 6) + __builtin_ctzll(b[i]));
	return (c);
}

static inline int	combo(int p, int ms, int q, int md, int *dir)
{
	int	fs = p, bs = ms - p, fd = q, bd = md - q, best, d = 0, c;

	best = fs > fd ? fs : fd;
	c = bs > bd ? bs : bd;
	if (c < best) { best = c; d = 1; }
	c = fs + bd;
	if (c < best) { best = c; d = 2; }
	c = bs + fd;
	if (c < best) { best = c; d = 3; }
	*dir = d;
	return (best);
}

static inline int	step(t_state *s, int sc, int dc, int sdom, int ddom,
		int *pp, int *qq, int *dd)
{
	int	p = cyc(s->src, s->stop, sc, sdom);
	int	q = s->dsz ? cyc(s->dst, s->dtop, dc, ddom) : 0;
	int	c = combo(p, s->ssz, q, s->dsz, dd);
	int	saved = 0;

	/* one step forward then one back around a push is a zigzag that a swap
	** replaces with one op less; emit_final writes the swap */
	if ((s->zz & 1) && q == 0 && s->ssz - p == 1 && c == 1)
		saved = 1;
	else if ((s->zz & 2) && p == 0 && s->dsz - q == 1 && c == 1)
		saved = 1;
	s->zz = 0;
	if (!saved && c == 1 && q == 0 && p == 1)
		s->zz = 1;
	else if (!saved && c == 1 && p == 0 && q == 1)
		s->zz = 2;
	*pp = p;
	*qq = q;
	s->src[sc >> 6] &= ~(1ULL << (sc & 63));
	s->ssz--;
	if (s->ssz)
		s->stop = next_set(s->src, sc, sdom);
	s->dst[dc >> 6] |= 1ULL << (dc & 63);
	s->dsz++;
	s->dtop = dc;
	return (c + 1 - saved);
}

static inline int	step0(t_state *s, int x, int *pp, int *qq, int *dd)
{
	int	pu, p, c;

	if (x < g_n)
		return (step(s, g_ipos[x], g_pipos[x], g_n, g.m, pp, qq, dd));
	pu = g_pu[x - g_n];
	p = cyc(s->src, s->stop, pu, g_n);
	c = p <= s->ssz - p ? p : s->ssz - p;
	*pp = p;
	*qq = -1;
	*dd = p <= s->ssz - p ? 0 : 1;
	s->stop = pu;
	s->zz = 0;
	if (next_set(s->src, (pu + 1) % g_n, g_n) != g_pv[x - g_n])
		c += 1000;
	return (c + 1);
}

static const char	*g_buf[MX * 40];
static int			g_blen;

static void	emit(const char *op, int cnt)
{
	while (g_out && cnt-- > 0)
		g_buf[g_blen++] = op;
}

static int	is_op(int i, const char *op)
{
	return (i < g_blen && !strcmp(g_buf[i], op));
}

/* rewrite each zigzag with its swap and print the solution */
static int	emit_final(void)
{
	int	i, w = 0;

	for (i = 0; i < g_blen; i++)
	{
		if (is_op(i, "ra") && is_op(i + 1, "pb") && is_op(i + 2, "rra") && is_op(i + 3, "pb"))
		{
			g_buf[w++] = "sa"; g_buf[w++] = "pb"; g_buf[w++] = "pb"; i += 3;
		}
		else if (is_op(i, "rb") && is_op(i + 1, "pb") && is_op(i + 2, "rrb") && is_op(i + 3, "pb"))
		{
			g_buf[w++] = "pb"; g_buf[w++] = "sb"; g_buf[w++] = "pb"; i += 3;
		}
		else if (is_op(i, "rb") && is_op(i + 1, "pa") && is_op(i + 2, "rrb") && is_op(i + 3, "pa"))
		{
			g_buf[w++] = "sb"; g_buf[w++] = "pa"; g_buf[w++] = "pa"; i += 3;
		}
		else if (is_op(i, "ra") && is_op(i + 1, "pa") && is_op(i + 2, "rra") && is_op(i + 3, "pa"))
		{
			g_buf[w++] = "pa"; g_buf[w++] = "sa"; g_buf[w++] = "pa"; i += 3;
		}
		else
			g_buf[w++] = g_buf[i];
	}
	for (i = 0; i < w; i++)
		puts(g_buf[i]);
	return (w);
}

static void	emit_move(int phase, int p, int ms, int q, int md, int d)
{
	const char	*fs = phase ? "rb" : "ra", *fd = phase ? "ra" : "rb";
	const char	*bs = phase ? "rrb" : "rra", *bd = phase ? "rra" : "rrb";
	int			a, b, o;

	a = (d == 0 || d == 2) ? p : ms - p;
	b = (d == 0 || d == 3) ? q : md - q;
	if (d == 0 || d == 1)
	{
		o = a < b ? a : b;
		emit(d == 0 ? "rr" : "rrr", o);
		a -= o;
		b -= o;
	}
	emit((d == 0 || d == 2) ? fs : bs, a);
	emit((d == 0 || d == 3) ? fd : bd, b);
	emit(phase ? "pa" : "pb", 1);
}

static void	start0(t_state *s)
{
	int	i;

	memset(s, 0, sizeof(*s));
	for (i = 0; i < g_n; i++)
		s->src[i >> 6] |= 1ULL << (i & 63);
	s->ssz = g_n;
}

static void	start1(t_state *s, const t_state *end0)
{
	int	i;

	memset(s, 0, sizeof(*s));
	for (i = 0; i < g.m; i++)
		s->src[i >> 6] |= 1ULL << (i & 63);
	for (i = 0; i < g_n; i++)
		if (g.keep[i] || g.defer[i])
			s->dst[ac(i) >> 6] |= 1ULL << (ac(i) & 63);
	s->ssz = g.m;
	s->dsz = g_n - g.m;
	s->stop = end0->dtop;
	s->dtop = ac(g_atc[end0->stop]);
}

static int	finish_cost(const t_state *s, int emitting)
{
	int	f = cyc(s->dst, s->dtop, 0, g_n);

	if (s->dtop == 0)
		f = 0;
	if (emitting)
		emit(f <= g_n - f ? "ra" : "rra", f <= g_n - f ? f : g_n - f);
	return (f <= g_n - f ? f : g_n - f);
}

static void	emit_move(int phase, int p, int ms, int q, int md, int d);

/* everything after T2: with FOUR, the deferred elements leave a in sig2 order
** (T3, a in g_ca coordinates, b in pi2 coordinates) and return in tau2 order
** (T4, a in rank coordinates); then the final rotation */
static int	g_t3cost;

static int	tail_run(const t_state *s1, int emitting)
{
	static t_state	s3, s4;
	int				i, c = 0, p, q, d, y, ms, md;

	if (!g_four)
		return (finish_cost(s1, emitting));
	memset(&s3, 0, sizeof(s3));
	for (i = 0; i < g_n; i++)
		s3.src[i >> 6] |= 1ULL << (i & 63);
	s3.ssz = g_n;
	s3.stop = s1->dtop;
	for (i = 0; i < g.m2; i++)
	{
		y = g.sig2[i];
		ms = s3.ssz;
		md = s3.dsz;
		c += step(&s3, g_ca[y], g_p2pos[y], g_n, g.m2, &p, &q, &d);
		if (emitting)
			emit_move(0, p, ms, q, md, d);
	}
	g_t3cost = c;
	memset(&s4, 0, sizeof(s4));
	for (i = 0; i < g.m2; i++)
		s4.src[i >> 6] |= 1ULL << (i & 63);
	for (i = 0; i < g_n; i++)
		if (!g.defer[i])
			s4.dst[i >> 6] |= 1ULL << (i & 63);
	s4.ssz = g.m2;
	s4.dsz = g_n - g.m2;
	s4.stop = s3.dtop;
	s4.dtop = g_ac[g.m2 ? s3.stop : s1->dtop];
	for (i = 0; i < g.m2; i++)
	{
		y = g.tau2[i];
		ms = s4.ssz;
		md = s4.dsz;
		c += step(&s4, g_p2pos[y], y, g.m2, g_n, &p, &q, &d);
		if (emitting)
			emit_move(1, p, ms, q, md, d);
	}
	return (c + finish_cost(&s4, emitting));
}

/* run a whole transfer; with ck/pre it refreshes the checkpoints */
static int	run0(t_state *s, t_state *ck, int *pre, int phase_emit)
{
	int	i, c = 0, p, q, d, x, ms, md;

	for (i = 0; i < g.ms; i++)
	{
		if (ck)
		{
			ck[i] = *s;
			pre[i] = c;
		}
		x = g.sigma[i];
		ms = s->ssz;
		md = s->dsz;
		c += step0(s, x, &p, &q, &d);
		if (phase_emit && x < g_n)
			emit_move(0, p, ms, q, md, d);
		else if (phase_emit)
		{
			emit(d == 0 ? "ra" : "rra", d == 0 ? p : ms - p);
			emit("sa", 1);
		}
	}
	if (ck)
	{
		ck[i] = *s;
		pre[i] = c;
	}
	return (c);
}

static int	run1(t_state *s, t_state *ck, int *pre, int phase_emit)
{
	int	i, c = 0, p, q, d, x, ms, md;

	for (i = 0; i < g.m; i++)
	{
		if (ck)
		{
			ck[i] = *s;
			pre[i] = c;
		}
		x = g.tau[i];
		ms = s->ssz;
		md = s->dsz;
		c += step(s, g_pipos[x], ac(x), g.m, g_n, &p, &q, &d);
		if (phase_emit)
			emit_move(1, p, ms, q, md, d);
	}
	if (ck)
	{
		ck[i] = *s;
		pre[i] = c;
	}
	return (c);
}

/* coordinates of a at the end of T2: each kept element, then the deferred
** elements that sit after it in a, then the ranks that pulls insert after it */
static void	recompute_ca(void)
{
	static int	hx[MX], tx[MX], hr[MX], tr[MX], nx[MX];
	int			k0 = -1, i, e, cur, c = 0, r;

	for (i = 0; i < g_n && k0 < 0; i++)
		if (g.keep[i])
			k0 = i;
	if (k0 < 0)
	{
		for (i = 0; i < g_n; i++)
			if (g.defer[g_init[i]])
			{
				g_ca[g_init[i]] = c;
				g_ac[c++] = g_init[i];
			}
		for (r = 0; r < g_n; r++)
			if (!g.defer[r])
			{
				g_ca[r] = c;
				g_ac[c++] = r;
			}
		return ;
	}
	for (i = 0; i < g_n; i++)
	{
		hx[i] = -1;
		tx[i] = -1;
		hr[i] = -1;
		tr[i] = -1;
	}
	cur = k0;
	for (i = 0; i < g_n; i++)
	{
		e = g_init[(g_ipos[k0] + i) % g_n];
		if (g.keep[e])
			cur = e;
		else if (g.defer[e])
		{
			nx[e] = -1;
			if (tx[cur] < 0)
				hx[cur] = e;
			else
				nx[tx[cur]] = e;
			tx[cur] = e;
		}
	}
	cur = k0;
	for (i = 0; i < g_n; i++)
	{
		e = (k0 + i) % g_n;
		if (g.keep[e])
			cur = e;
		else if (!g.defer[e])
		{
			nx[e] = -1;
			if (tr[cur] < 0)
				hr[cur] = e;
			else
				nx[tr[cur]] = e;
			tr[cur] = e;
		}
	}
	for (i = 0; i < g_n; i++)
	{
		r = (k0 + i) % g_n;
		if (!g.keep[r])
			continue ;
		g_ca[r] = c;
		g_ac[c++] = r;
		for (e = hx[r]; e >= 0; e = nx[e])
		{
			g_ca[e] = c;
			g_ac[c++] = e;
		}
		for (e = hr[r]; e >= 0; e = nx[e])
		{
			g_ca[e] = c;
			g_ac[c++] = e;
		}
	}
}

static void	reindex2(void)
{
	int	i;

	for (i = 0; i < g.m2; i++)
	{
		g_s2pos[g.sig2[i]] = i;
		g_p2pos[g.pi2[i]] = i;
		g_t2pos[g.tau2[i]] = i;
	}
}

static void	reindex(void)
{
	int	i;

	for (i = 0; i < g.m; i++)
	{
		g_pipos[g.pi[i]] = i;
		g_tpos[g.tau[i]] = i;
	}
	for (i = 0; i < g.ms; i++)
		g_spos[g.sigma[i]] = i;
	if (g_four)
	{
		recompute_ca();
		reindex2();
	}
}

static int	eval_full(int save)
{
	static t_state	s0, s1;
	int				c;

	start0(&s0);
	c = run0(&s0, save ? g_ck0 : NULL, g_pre0, 0);
	start1(&s1, &s0);
	c += run1(&s1, save ? g_ck1 : NULL, g_pre1, 0);
	c += save ? (g_finish = tail_run(&s1, 0)) : tail_run(&s1, 0);
	return (c);
}

static void	emit_solution(void)
{
	static t_state	s0, s1;

	start0(&s0);
	run0(&s0, NULL, NULL, 1);
	start1(&s1, &s0);
	run1(&s1, NULL, NULL, 1);
	tail_run(&s1, 1);
}

static void	greedy_tau(void)
{
	static t_state	s0, s1, t;
	int				i, j, p, q, d, x, best, bj, c;

	start0(&s0);
	run0(&s0, NULL, NULL, 0);
	start1(&s1, &s0);
	for (i = 0; i < g.m; i++)
	{
		best = 1 << 30;
		bj = i;
		for (j = i; j < g.m; j++)
		{
			x = g.tau[j];
			p = cyc(s1.src, s1.stop, g_pipos[x], g.m);
			q = cyc(s1.dst, s1.dtop, ac(x), g_n);
			c = combo(p, s1.ssz, q, s1.dsz, &d);
			if (c < best)
			{
				best = c;
				bj = j;
			}
		}
		x = g.tau[bj];
		g.tau[bj] = g.tau[i];
		g.tau[i] = x;
		t = s1;
		step(&t, g_pipos[x], ac(x), g.m, g_n, &p, &q, &d);
		s1 = t;
	}
}

/* best kept set, trying rotations: a kept element scores SC_ONE and an inverted
** pair, adjacent once the elements between have left, costs one sa */
#define SC_ONE 4
#define SC_PAIR 7

typedef struct s_kn
{
	int	parent;
	int	u;
	int	v;
}	t_kn;

static void	best_keep(const int *r, int off, int use_pairs, int *best, t_kn *pool)
{
	static int	dp[MX + 1], dn[MX + 1], pm[MX + 2], pn[MX + 2];
	static int	pend[MX + 1][MX + 1], pendn[MX + 1][MX + 1];
	int			np = 0, p, q, v, x, e = -1, sc = 0;

	for (v = 0; v <= g_n; v++)
	{
		dp[v] = -1;
		dn[v] = -1;
	}
	dp[0] = 0;
	for (p = 0; p <= g_n; p++)
		for (v = 0; v <= g_n; v++)
			pend[p][v] = -1;
	for (p = 0; p < g_n; p++)
	{
		for (v = 0; v <= g_n; v++)
			if (pend[p][v] > dp[v])
			{
				dp[v] = pend[p][v];
				dn[v] = pendn[p][v];
			}
		pm[0] = dp[0];
		pn[0] = dn[0];
		for (v = 1; v <= g_n; v++)
		{
			pm[v] = pm[v - 1];
			pn[v] = pn[v - 1];
			if (dp[v] > pm[v])
			{
				pm[v] = dp[v];
				pn[v] = dn[v];
			}
		}
		x = r[p];
		if (pm[x] + SC_ONE > dp[x + 1])
		{
			pool[np] = (t_kn){pn[x], (p + off) % g_n, -1};
			dp[x + 1] = pm[x] + SC_ONE;
			dn[x + 1] = np++;
		}
		if (!use_pairs)
			continue ;
		for (q = p + 1; q < g_n; q++)
			if (r[q] < x && pm[r[q]] + SC_PAIR > pend[q + 1][x + 1])
			{
				pool[np] = (t_kn){pn[r[q]], (p + off) % g_n, (q + off) % g_n};
				pend[q + 1][x + 1] = pm[r[q]] + SC_PAIR;
				pendn[q + 1][x + 1] = np++;
			}
	}
	for (v = 0; v <= g_n; v++)
	{
		if (pend[g_n][v] > dp[v])
		{
			dp[v] = pend[g_n][v];
			dn[v] = pendn[g_n][v];
		}
		if (dp[v] > sc)
		{
			sc = dp[v];
			e = dn[v];
		}
	}
	if (sc <= *best)
		return ;
	*best = sc;
	memset(g.keep, 0, sizeof(g.keep));
	memcpy(g_atc, g_init, sizeof(int) * g_n);
	g_npairs = 0;
	memset(g_inpair, 0, sizeof(g_inpair));
	for (; e >= 0; e = pool[e].parent)
	{
		g.keep[g_init[pool[e].u]] = 1;
		if (pool[e].v < 0)
			continue ;
		g.keep[g_init[pool[e].v]] = 1;
		g_inpair[g_init[pool[e].u]] = 1;
		g_inpair[g_init[pool[e].v]] = 1;
		g_atc[pool[e].u] = g_init[pool[e].v];
		g_atc[pool[e].v] = g_init[pool[e].u];
		g_pu[g_npairs] = pool[e].u;
		g_pv[g_npairs++] = pool[e].v;
	}
}

/* KEEPV=1 races every start twice: with the plain keep and with the keep
** that may hold inverted pairs, fixed by sa as the push sweep passes them */
typedef struct s_keepset
{
	char	keep[MX];
	int		npairs;
	int		pu[64];
	int		pv[64];
	char	inpair[MX];
	int		atc[MX];
}	t_keepset;

static int			g_force_pairs = -1;
static t_keepset	g_ks[2];

static void	save_ks(int v)
{
	int	i;

	for (i = 0; i < g_n; i++)
		g_ks[v].keep[i] = g.keep[i];
	g_ks[v].npairs = g_npairs;
	memcpy(g_ks[v].pu, g_pu, sizeof(g_pu));
	memcpy(g_ks[v].pv, g_pv, sizeof(g_pv));
	memcpy(g_ks[v].inpair, g_inpair, sizeof(g_inpair));
	memcpy(g_ks[v].atc, g_atc, sizeof(int) * g_n);
}

/* the pair globals of set v; with set_keep also its keep set in g */
static void	load_ks(int v, int set_keep)
{
	int	i;

	if (set_keep)
		for (i = 0; i < g_n; i++)
			g.keep[i] = g_ks[v].keep[i];
	g_npairs = g_ks[v].npairs;
	memcpy(g_pu, g_ks[v].pu, sizeof(g_pu));
	memcpy(g_pv, g_ks[v].pv, sizeof(g_pv));
	memcpy(g_inpair, g_ks[v].inpair, sizeof(g_inpair));
	memcpy(g_atc, g_ks[v].atc, sizeof(int) * g_n);
}

static void	init_keep(void)
{
	static int	r[MX];
	static t_kn	*pool;
	int			off, i, best = 0, use_pairs, stepo;

	if (!pool)
		pool = malloc(sizeof(t_kn) * (MX + 1) * (MX + 1));
	use_pairs = getenv("PAIRS") ? atoi(getenv("PAIRS")) : 0;
	if (g_force_pairs >= 0)
		use_pairs = g_force_pairs;
	stepo = g_n > 200 ? g_n / 25 : 1;
	for (off = 0; off < g_n; off += stepo)
	{
		for (i = 0; i < g_n; i++)
			r[i] = (g_init[(i + off) % g_n] - g_init[off] + g_n) % g_n;
		best_keep(r, off, use_pairs, &best, pool);
	}
}

/* generic start: lap[x] says in which sweep of a the element leaves, and
** side[x] whether it goes to the top of b or, through rb, to the bottom.
** Within a sweep the nearest pending element of that sweep goes first */
static void	init_laps(const int *lap, const int *side, int nlaps)
{
	static int	alive[MX], tops[MX], bots[MX];
	int			c, top = 0, nt = 0, nb = 0, i, x, f, bf, bd;

	g.m = 0;
	for (i = 0; i < g_n; i++)
		alive[i] = 1;
	for (c = 0; c < nlaps; c++)
	{
		while (1)
		{
			bf = -1;
			bd = 1 << 30;
			f = 0;
			for (i = 0; i < g_n; i++)
			{
				int	at = (top + i) % g_n;

				if (!alive[at])
					continue ;
				x = g_init[at];
				if (!g.keep[x] && lap[x] == c && f < bd)
				{
					bd = f;
					bf = at;
				}
				f++;
			}
			for (i = 0, f = 0; i < g_n; i++)
			{
				int	at = (top - i - 1 + 2 * g_n) % g_n;

				if (!alive[at])
					continue ;
				f++;
				x = g_init[at];
				if (!g.keep[x] && lap[x] == c && f < bd)
				{
					bd = f;
					bf = at;
				}
			}
			if (bf < 0)
				break ;
			x = g_init[bf];
			g.sigma[g.m++] = x;
			if (side[x])
				bots[nb++] = x;
			else
				tops[nt++] = x;
			alive[bf] = 0;
			top = bf;
		}
	}
	for (i = 0; i < nt; i++)
		g.pi[i] = tops[nt - 1 - i];
	for (i = 0; i < nb; i++)
		g.pi[nt + i] = bots[i];
	memcpy(g.tau, g.sigma, sizeof(int) * g.m);
	g.ms = g.m;
	for (i = 0; i < g_npairs; i++)
		g.sigma[g.ms++] = g_n + i;
	reindex();
	greedy_tau();
	reindex();
}

/* sweep index for a position t in [0, 1) when nlaps sweeps get unequal
** widths: gamma below 1 makes the first sweeps wider, above 1 narrower */
static int	lap_of(double t, int nlaps, double gamma)
{
	int	l = (int)(nlaps * (1.0 - pow(1.0 - t, gamma)));

	if (l >= nlaps)
		l = nlaps - 1;
	return (l < 0 ? 0 : l);
}

/* the original start: k rank bands in turn, the lower half of each band to
** the bottom of b. Across bands this leaves b folded in a V or an inverted V */
static void	init_bands(int k, int off, int rev, double gamma)
{
	static int	lap[MX], side[MX];
	int			x, sh, c, lo, hi;

	for (x = 0; x < g_n; x++)
	{
		sh = (x - off + g_n) % g_n;
		c = lap_of((double)sh / g_n, k, gamma);
		lo = (int)ceil(g_n * (1.0 - pow(1.0 - (double)c / k, 1.0 / gamma)));
		hi = (int)ceil(g_n * (1.0 - pow(1.0 - (double)(c + 1) / k, 1.0 / gamma)));
		side[x] = sh < (lo + hi) / 2;
		lap[x] = rev ? k - 1 - c : c;
	}
	init_laps(lap, side, k);
}

/* a full ring around rank centre: sweep j pushes the ranks at distance j from
** the centre on both sides. desc sends the upper side to the top of b and the
** lower side to the bottom, so b ends descending from top to bottom and pulls
** chain without rotations; otherwise b ends ascending and pulls share rr */
static void	init_ring(int nlaps, int centre, int desc, double gamma)
{
	static int	lap[MX], side[MX];
	int			x, sh, upper, dist;

	for (x = 0; x < g_n; x++)
	{
		sh = (x - centre + g_n) % g_n;
		upper = sh < g_n / 2;
		dist = upper ? sh : g_n - sh;
		lap[x] = lap_of((double)dist / (g_n / 2 + 1), nlaps, gamma);
		side[x] = desc ? !upper : upper;
	}
	init_laps(lap, side, nlaps);
}

/* relocate arr[from..from+len) so it starts at index `to` of the final array */
static void	relocate(int *arr, int from, int len, int to)
{
	static int	buf[MX];

	memcpy(buf, arr + from, sizeof(int) * len);
	if (to < from)
		memmove(arr + to + len, arr + to, sizeof(int) * (from - to));
	else
		memmove(arr + from, arr + from + len, sizeof(int) * (to - from));
	memcpy(arr + to, buf, sizeof(int) * len);
}

static void	reverse(int *arr, int i, int j)
{
	int	t;

	while (i < j)
	{
		t = arr[i];
		arr[i++] = arr[j];
		arr[j--] = t;
	}
}

static inline int	pick_dist(int m)
{
	unsigned int	r = rnd() % 100;

	if (r < 60)
		return (1 + rnd() % 8);
	if (r < 90)
		return (1 + rnd() % 40);
	return (1 + rnd() % (m > 1 ? m : 1));
}

/* a random relocation target, or next to a neighbour of arr[from] */
static int	pick_target(int m, int from, int len)
{
	int	d = pick_dist(m), to;

	to = (rnd() & 1) ? from + d : from - d;
	if (to < 0)
		to = 0;
	if (to > m - len)
		to = m - len;
	return (to);
}

static int	guided_target(const int *arr, const int *pos, int m, int from, int len,
		int key_near)
{
	int	x = key_near, to;

	(void)arr;
	to = pos[x];
	if (to > from)
		to = to - len + 1;
	else
		to = to + 1;
	if (rnd() & 1)
		to -= 1;
	if (to < 0)
		to = 0;
	if (to > m - len)
		to = m - len;
	return (to);
}

/* an element of the genome close to x in rank; -1 when none found */
static int	rank_neighbour(int x)
{
	int	d = 1 + rnd() % 4, y, tries;

	if (rnd() & 1)
		d = -d;
	for (tries = 0; tries < 6; tries++)
	{
		y = (x + d + g_n) % g_n;
		if (!g.keep[y] && !g.defer[y])
			return (y);
		d += d > 0 ? 1 : -1;
	}
	return (-1);
}

/* an element close to x in the initial order of a; -1 when none found */
static int	pos_neighbour(int x)
{
	int	d = 1 + rnd() % 4, y, tries;

	if (rnd() & 1)
		d = -d;
	for (tries = 0; tries < 6; tries++)
	{
		y = g_init[(g_ipos[x] + d + g_n) % g_n];
		if (!g.keep[y] && !g.defer[y])
			return (y);
		d += d > 0 ? 1 : -1;
	}
	return (-1);
}

/* keep must stay cyclically increasing along the initial order */
static int	keep_ok(void)
{
	int	i, prev = -1, first = -1, drops = 0, cnt = 0;

	for (i = 0; i < g_n; i++)
		if (g.keep[g_atc[i]])
		{
			if (first < 0)
				first = g_atc[i];
			if (prev >= 0 && g_atc[i] < prev)
				drops++;
			prev = g_atc[i];
			cnt++;
		}
	if (prev >= 0 && first < prev)
		drops++;
	return (drops <= 1 && cnt >= 1);
}

static void	arr_remove(int *a, int m, int v)
{
	int	i = 0;

	while (a[i] != v)
		i++;
	memmove(a + i, a + i + 1, sizeof(int) * (m - i - 1));
}

static void	arr_insert(int *a, int m, int v, int at)
{
	memmove(a + at + 1, a + at, sizeof(int) * (m - at));
	a[at] = v;
}

static double	now(void)
{
	struct timespec	t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (t.tv_sec + t.tv_nsec * 1e-9);
}

/* simulate positions lo..hi of a transfer; returns the cost of that range */
static inline int	range_cost(const t_state *ck, const int *ord, int lo, int hi,
		int phase, t_state *out)
{
	t_state	s = ck[lo];
	int		i, c = 0, p, q, d, x;

	for (i = lo; i <= hi; i++)
	{
		x = ord[i];
		if (phase == 0)
			c += step0(&s, x, &p, &q, &d);
		else
			c += step(&s, g_pipos[x], ac(x), g.m, g_n, &p, &q, &d);
	}
	*out = s;
	return (c);
}

static void	refresh_range(t_state *ck, int *pre, const int *ord, int lo, int hi,
		int phase)
{
	t_state	s = ck[lo];
	int		i, c = pre[lo], p, q, d, x;

	for (i = lo; i <= hi; i++)
	{
		ck[i] = s;
		pre[i] = c;
		x = ord[i];
		if (phase == 0)
			c += step0(&s, x, &p, &q, &d);
		else
			c += step(&s, g_pipos[x], ac(x), g.m, g_n, &p, &q, &d);
	}
	ck[hi + 1] = s;
	pre[hi + 1] = c;
}

typedef struct s_stats
{
	long	iters;
	long	acc;
	double	secs;
}	t_stats;

#define ABORT (1 << 29)
#define LOGU 4096

static double	g_logu[LOGU];
static double	g_temp;
static int		g_d;
static int		g_limit;
static int		g_slack = 8;
static int		g_tourn = 1;
static int		g_long = 0;
static int		g_rate_pi = 40;
static int		g_rate_tog = 10;
static int		g_rate_pair = 40;

/* Metropolis with the random draw made first: the trial knows the largest
** delta it may end with, so a clearly lost simulation can stop early */
static inline void	draw_threshold(void)
{
	g_d = (int)floor(g_temp * g_logu[rnd() % LOGU]);
	g_limit = g_d + g_slack;
}

static inline int	accept(int delta)
{
	return (delta <= g_d);
}

/* delta over steps lo..hi against the stored prefix costs, or ABORT */
static int	run_bounded(t_state *s, const int *ord, int lo, int hi, int phase,
		const int *pre, int base)
{
	int	i, c = 0, p, q, d, x;

	for (i = lo; i <= hi; i++)
	{
		x = ord[i];
		if (phase == 0)
			c += step0(s, x, &p, &q, &d);
		else
			c += step(s, g_pipos[x], ac(x), g.m, g_n, &p, &q, &d);
		if (base + c - (pre[i + 1] - pre[lo]) > g_limit)
			return (ABORT);
	}
	return (base + c - (pre[hi + 1] - pre[lo]));
}

/* express a checkpointed b in the coordinates of the new pi; only positions
** lo..hi moved, and old[i] is the element that held position i */
static void	relabel_dst(t_state *s, const int *old, int lo, int hi)
{
	static char	bit[MX];
	int			i, c;

	for (i = lo; i <= hi; i++)
	{
		bit[i] = (s->dst[i >> 6] >> (i & 63)) & 1;
		s->dst[i >> 6] &= ~(1ULL << (i & 63));
	}
	for (i = lo; i <= hi; i++)
		if (bit[i])
		{
			c = g_pipos[old[i]];
			s->dst[c >> 6] |= 1ULL << (c & 63);
		}
	if (s->dtop >= lo && s->dtop <= hi)
		s->dtop = g_pipos[old[s->dtop]];
}

/* local reorder of sigma or tau, evaluated only on the affected range */
static int	try_local(int phase, int *cur)
{
	static int	save[MX];
	static t_state	tail;
	int		*ord = phase ? g.tau : g.sigma;
	int		*pos = phase ? g_tpos : g_spos;
	t_state	*ck = phase ? g_ck1 : g_ck0;
	int		*pre = phase ? g_pre1 : g_pre0;
	int		m = phase ? g.m : g.ms, lo, hi, end, old, c, delta, i, r, extra = 0;
	t_state	out;

	if (m < 4)
		return (0);
	r = rnd() % 100;
	if (r < 12)
	{
		int	d = pick_dist(m);

		lo = rnd() % m;
		hi = lo + d;
		if (hi >= m)
			hi = m - 1;
		memcpy(save + lo, ord + lo, sizeof(int) * (hi - lo + 1));
		reverse(ord, lo, hi);
	}
	else
	{
		int	len = 1 + rnd() % 3, from, to, y;

		if (g_long && rnd() % 100 < g_long)
			len = 4 + rnd() % 28;
		if (len > m / 2)
			len = 1;
		from = rnd() % (m - len + 1);
		for (i = 1; i < g_tourn; i++)
		{
			int	alt = rnd() % (m - len + 1);

			if (pre[alt + 1] - pre[alt] > pre[from + 1] - pre[from])
				from = alt;
		}
		y = -1;
		if (r < 45)
			y = phase ? rank_neighbour(ord[from])
				: (ord[from] < g_n ? pos_neighbour(ord[from]) : -1);
		if (y >= 0 && (pos[y] < from || pos[y] >= from + len))
			to = guided_target(ord, pos, m, from, len, y);
		else
			to = pick_target(m, from, len);
		if (to == from)
			return (0);
		lo = from < to ? from : to;
		hi = (from > to ? from : to) + len - 1;
		memcpy(save + lo, ord + lo, sizeof(int) * (hi - lo + 1));
		relocate(ord, from, len, to);
	}
	end = hi + 2 < m ? hi + 2 : m - 1;
	old = pre[end + 1] - pre[lo];
	c = range_cost(ck, ord, lo, end, phase, &out);
	if (end == m - 1)
	{
		if (phase)
			extra = tail_run(&out, 0) - g_finish;
		else
		{
			static t_state	s1;

			start1(&s1, &out);
			tail = s1;
			extra = run1(&tail, NULL, NULL, 0) + tail_run(&tail, 0)
				- (g_pre1[g.m] + g_finish);
		}
	}
	delta = c - old + extra;
	if (!accept(delta))
	{
		memcpy(ord + lo, save + lo, sizeof(int) * (hi - lo + 1));
		return (0);
	}
	for (i = lo; i <= hi; i++)
		pos[ord[i]] = i;
	if (end == m - 1 && !phase)
		*cur = eval_full(1);
	else
	{
		refresh_range(ck, pre, ord, lo, end, phase);
		if (end < m - 1)
			for (i = end + 2; i <= m; i++)
				pre[i] += c - old;
		else
			g_finish = tail_run(&ck[m], 0);
		*cur += delta;
	}
	return (1);
}

/* a reorder of pi: every coordinate of b may shift, so both transfers rerun */
static int	try_pi(int *cur)
{
	static int		save[MX];
	static t_state	s0, s1;
	int				m = g.m, len = 1 + rnd() % 3, from, to, y, lo, hi, i, c;
	int				first = m, last = 0, delta;

	if (m < 4)
		return (0);
	from = rnd() % (m - len + 1);
	y = rnd() % 100 < 60 ? rank_neighbour(g.pi[from]) : -1;
	if (y >= 0 && (g_pipos[y] < from || g_pipos[y] >= from + len))
		to = guided_target(g.pi, g_pipos, m, from, len, y);
	else
		to = pick_target(m, from, len);
	if (to == from)
		return (0);
	lo = from < to ? from : to;
	hi = (from > to ? from : to) + len - 1;
	memcpy(save + lo, g.pi + lo, sizeof(int) * (hi - lo + 1));
	relocate(g.pi, from, len, to);
	for (i = lo; i <= hi; i++)
		g_pipos[g.pi[i]] = i;
	/* only the moved elements change relative order: pushes before the first
	** of them cost the same, and pulls after the last of them too, except
	** the next one, whose starting top in b may differ */
	for (i = to; i < to + len; i++)
	{
		if (g_spos[g.pi[i]] < first)
			first = g_spos[g.pi[i]];
		if (g_tpos[g.pi[i]] > last)
			last = g_tpos[g.pi[i]];
	}
	s0 = g_ck0[first];
	relabel_dst(&s0, save, lo, hi);
	delta = run_bounded(&s0, g.sigma, first, g.ms - 1, 0, g_pre0, 0);
	if (delta != ABORT)
	{
		start1(&s1, &s0);
		delta = run_bounded(&s1, g.tau, 0, last + 2 < m ? last + 2 : m - 1,
				1, g_pre1, delta);
	}
	if (delta == ABORT || !accept(delta))
	{
		memcpy(g.pi + lo, save + lo, sizeof(int) * (hi - lo + 1));
		for (i = lo; i <= hi; i++)
			g_pipos[g.pi[i]] = i;
		return (0);
	}
	c = eval_full(1);
	if (c != *cur + delta && getenv("CHECK"))
		fprintf(stderr, "pi delta mismatch %d %d\n", c, *cur + delta);
	*cur = c;
	return (1);
}

static void	place_beside(int *arr, const int *pos, int x, int y, int after)
{
	int	from = pos[x], py = pos[y], to;

	if (from < py)
		to = after ? py : py - 1;
	else
		to = after ? py + 1 : py;
	if (to != from)
		relocate(arr, from, 1, to);
}

/* compound move: x goes next to a neighbour y both in pi and in one order, so
** the two consecutive transfers of x and y become cheap together. mode 0
** pairs rank neighbours in pi and tau, mode 1 pairs a neighbours in pi and sigma */
static int	try_pair(int mode, int *cur)
{
	static int		save_pi[MX], save_ord[MXI];
	static t_state	s0, s1;
	int				*ord = mode ? g.sigma : g.tau;
	int				*pos = mode ? g_spos : g_tpos;
	int				x, y, m = g.m, delta;

	if (m < 4)
		return (0);
	x = g.pi[rnd() % m];
	y = mode ? pos_neighbour(x) : rank_neighbour(x);
	if (y < 0 || y == x)
		return (0);
	memcpy(save_pi, g.pi, sizeof(int) * m);
	memcpy(save_ord, ord, sizeof(int) * (mode ? g.ms : m));
	place_beside(g.pi, g_pipos, x, y, rnd() & 1);
	place_beside(ord, pos, x, y, rnd() & 1);
	reindex();
	start0(&s0);
	delta = run_bounded(&s0, g.sigma, 0, g.ms - 1, 0, g_pre0, 0);
	if (delta != ABORT)
	{
		start1(&s1, &s0);
		delta = run_bounded(&s1, g.tau, 0, m - 1, 1, g_pre1, delta);
	}
	if (delta != ABORT)
		delta += tail_run(&s1, 0) - g_finish;
	if (delta == ABORT || !accept(delta))
	{
		memcpy(g.pi, save_pi, sizeof(int) * m);
		memcpy(ord, save_ord, sizeof(int) * (mode ? g.ms : m));
		reindex();
		return (0);
	}
	*cur = eval_full(1);
	return (1);
}

/* move one element into or out of the keep set */
static int	try_toggle(int *cur)
{
	static t_gene	save;
	int				x = rnd() % g_n, c;

	if (g_inpair[x] || g.defer[x])
		return (0);
	save = g;
	if (g.keep[x])
	{
		g.keep[x] = 0;
		if (!keep_ok())
		{
			g.keep[x] = 1;
			return (0);
		}
		arr_insert(g.sigma, g.ms, x, rnd() % (g.ms + 1));
		g.ms++;
		arr_insert(g.pi, g.m, x, rnd() % (g.m + 1));
		arr_insert(g.tau, g.m, x, rnd() % (g.m + 1));
		g.m++;
	}
	else
	{
		g.keep[x] = 1;
		if (!keep_ok() || g.m < 2)
		{
			g.keep[x] = 0;
			return (0);
		}
		arr_remove(g.sigma, g.ms, x);
		g.ms--;
		arr_remove(g.pi, g.m, x);
		arr_remove(g.tau, g.m, x);
		g.m--;
	}
	reindex();
	c = eval_full(0);
	if (!accept(c - *cur))
	{
		g = save;
		reindex();
		return (0);
	}
	*cur = eval_full(1);
	return (1);
}

static int		g_rate_rr = 0;
static int		g_rr_k = 6;
static char		g_pend[MX];

/* moves x, already in arr, to the cheapest of a few guided spots: next to its
** rank neighbours (or its neighbours in a, for sigma) and two random ones */
static void	place_best(int *arr, int *pos, int m, int x, int use_pos_nb)
{
	int	cand[16], nc = 0, i, y, d, c, at, best_c = 1 << 30, best_at = pos[x];

	for (d = -3; d <= 3 && nc < 12; d++)
	{
		if (!d)
			continue ;
		y = use_pos_nb ? g_init[(g_ipos[x] + d + g_n) % g_n] : (x + d + g_n) % g_n;
		if (y == x || g.keep[y] || g_pend[y])
			continue ;
		cand[nc++] = pos[y];
		cand[nc++] = pos[y] + 1;
	}
	cand[nc++] = rnd() % m;
	cand[nc++] = rnd() % m;
	for (i = 0; i < nc; i++)
	{
		at = cand[i] < m ? cand[i] : m - 1;
		if (at != pos[x])
		{
			relocate(arr, pos[x], 1, at);
			reindex();
		}
		c = eval_full(0);
		if (c < best_c)
		{
			best_c = c;
			best_at = at;
		}
	}
	if (best_at != pos[x])
	{
		relocate(arr, pos[x], 1, best_at);
		reindex();
	}
}

/* ruin and recreate: take out a few elements close in rank, kept ones too,
** and put each back at the best guided spot in all three orders */
static int	try_rr(int *cur)
{
	static t_gene	save;
	static int		chosen[16];
	int				k = 2 + rnd() % (g_rr_k - 1), e = rnd() % g_n, nc = 0, d, i, x, t;

	save = g;
	for (d = 0; nc < k && d < g_n; d++)
		chosen[nc++] = (e + ((d & 1) ? (d + 1) / 2 : -(d / 2)) + g_n) % g_n;
	for (i = 0; i < nc; i++)
	{
		x = chosen[i];
		if (g.keep[x] && g_n - g.m <= 2)
			chosen[i] = -1;
		else if (g.keep[x])
			g.keep[x] = 0;
		else
		{
			arr_remove(g.sigma, g.ms, x);
			g.ms--;
			arr_remove(g.pi, g.m, x);
			arr_remove(g.tau, g.m, x);
			g.m--;
		}
		if (chosen[i] >= 0)
			g_pend[x] = 1;
	}
	for (i = nc - 1; i > 0; i--)
	{
		d = rnd() % (i + 1);
		t = chosen[i];
		chosen[i] = chosen[d];
		chosen[d] = t;
	}
	for (i = 0; i < nc; i++)
	{
		if ((x = chosen[i]) < 0)
			continue ;
		arr_insert(g.sigma, g.ms, x, rnd() % (g.ms + 1));
		g.ms++;
		arr_insert(g.pi, g.m, x, rnd() % (g.m + 1));
		arr_insert(g.tau, g.m, x, rnd() % (g.m + 1));
		g.m++;
		g_pend[x] = 0;
		reindex();
		place_best(g.tau, g_tpos, g.m, x, 0);
		place_best(g.pi, g_pipos, g.m, x, 0);
		place_best(g.sigma, g_spos, g.ms, x, 1);
	}
	if (!accept(eval_full(0) - *cur))
	{
		g = save;
		reindex();
		return (0);
	}
	*cur = eval_full(1);
	return (1);
}

static int		g_rr_fast = 0;

/* best slot for x in tau, as an index of tau without x. Each slot is scored
** on the checkpoints by simulating only the pull of x and of the element that
** follows it; everything further on is assumed unchanged */
static int	slot_tau(int x)
{
	int		m = g.m, tx = g_tpos[x], j, k, y, c, best = 1 << 30, bj = tx;
	int		p, q, d;
	t_state	s;

	for (j = 0; j < m; j++)
	{
		k = j < tx ? j : j + 1;
		if (j == tx)
			k = tx;
		s = g_ck1[k];
		if (k > tx)
		{
			s.src[g_pipos[x] >> 6] |= 1ULL << (g_pipos[x] & 63);
			s.ssz++;
			s.dst[ac(x) >> 6] &= ~(1ULL << (ac(x) & 63));
			s.dsz--;
			s.stop = next_set(s.src, g_pipos[g.tau[k - 1]], m);
		}
		c = step(&s, g_pipos[x], ac(x), m, g_n, &p, &q, &d);
		y = -1;
		if (j == tx && tx + 1 < m)
			y = g.tau[tx + 1];
		else if (j != tx && k < m && g.tau[k] != x)
			y = g.tau[k];
		if (y >= 0)
		{
			int	ky = g_tpos[y];

			c += step(&s, g_pipos[y], ac(y), m, g_n, &p, &q, &d)
				- (g_pre1[ky + 1] - g_pre1[ky]);
		}
		if (c < best)
		{
			best = c;
			bj = j;
		}
	}
	return (bj);
}

/* the same for sigma, with the push of x and of the element that follows */
static int	slot_sigma(int x)
{
	int		m = g.ms, sx = g_spos[x], j, k, y, c, best = 1 << 30, bj = sx;
	int		p, q, d;
	t_state	s;

	for (j = 0; j < m; j++)
	{
		k = j < sx ? j : j + 1;
		if (j == sx)
			k = sx;
		s = g_ck0[k];
		if (k > sx)
		{
			s.src[g_ipos[x] >> 6] |= 1ULL << (g_ipos[x] & 63);
			s.ssz++;
			s.dst[g_pipos[x] >> 6] &= ~(1ULL << (g_pipos[x] & 63));
			s.dsz--;
			s.stop = next_set(s.src, g_ipos[g.sigma[k - 1]], g_n);
		}
		c = step0(&s, x, &p, &q, &d);
		y = -1;
		if (j == sx && sx + 1 < m)
			y = g.sigma[sx + 1];
		else if (j != sx && k < m && g.sigma[k] != x)
			y = g.sigma[k];
		if (y >= 0)
		{
			int	ky = g_spos[y];

			c += step0(&s, y, &p, &q, &d) - (g_pre0[ky + 1] - g_pre0[ky]);
		}
		if (c < best)
		{
			best = c;
			bj = j;
		}
	}
	return (bj);
}

/* best index for x in pi: the coordinate only changes the rotations of b in
** the push and in the pull of x itself, so those two are scored */
static int	slot_pi(int x)
{
	int		m = g.m, P = g_pipos[x], c, best = 1 << 30, bj = P, est, d;
	int		p0, q0, stop;
	t_state	a = g_ck0[g_spos[x]], b = g_ck1[g_tpos[x]];

	p0 = cyc(a.src, a.stop, g_ipos[x], g_n);
	q0 = cyc(b.dst, b.dtop, ac(x), g_n);
	b.src[P >> 6] &= ~(1ULL << (P & 63));
	stop = b.stop;
	if (stop == P && b.ssz > 1)
		stop = next_set(b.src, (P + 1) % m, m);
	for (c = 0; c < m; c++)
	{
		if (c == P)
			continue ;
		est = combo(p0, a.ssz, a.dsz ? cyc(a.dst, a.dtop, c, m) : 0, a.dsz, &d);
		if (b.ssz > 1)
			est += combo(cyc(b.src, stop, c, m), b.ssz, q0, b.dsz, &d);
		else
			est += combo(0, 1, q0, b.dsz, &d);
		if (est < best)
		{
			best = est;
			bj = c < P ? c : c - 1;
		}
	}
	return (bj);
}

/* fast ruin and recreate: each element goes back in at a guided spot, then
** moves to the best slot of each order by the estimates above. One full
** simulation per element refreshes the checkpoints */
static int	try_rr_fast(int *cur)
{
	static t_gene	save;
	static int		chosen[16];
	int				k = 2 + rnd() % (g_rr_k - 1), e = rnd() % g_n, nc = 0;
	int				d, i, x, t, y, at;

	if (g_npairs)
		return (0);
	save = g;
	for (d = 0; nc < k && d < g_n; d++)
		chosen[nc++] = (e + ((d & 1) ? (d + 1) / 2 : -(d / 2)) + g_n) % g_n;
	for (i = 0; i < nc; i++)
	{
		x = chosen[i];
		if (g.keep[x] && g_n - g.m <= 2)
			chosen[i] = -1;
		else if (g.keep[x])
			g.keep[x] = 0;
		else
		{
			arr_remove(g.sigma, g.ms, x);
			g.ms--;
			arr_remove(g.pi, g.m, x);
			arr_remove(g.tau, g.m, x);
			g.m--;
		}
	}
	for (i = nc - 1; i > 0; i--)
	{
		d = rnd() % (i + 1);
		t = chosen[i];
		chosen[i] = chosen[d];
		chosen[d] = t;
	}
	for (i = 0; i < nc; i++)
	{
		if ((x = chosen[i]) < 0)
			continue ;
		reindex();
		y = (x + g_n - 1) % g_n;
		at = (!g.keep[y] && g_tpos[y] < g.m && g.tau[g_tpos[y]] == y)
			? g_tpos[y] + 1 : rnd() % (g.m + 1);
		arr_insert(g.tau, g.m, x, at);
		at = (!g.keep[y] && g_pipos[y] < g.m && g.pi[g_pipos[y]] == y)
			? g_pipos[y] + 1 : rnd() % (g.m + 1);
		arr_insert(g.pi, g.m, x, at);
		arr_insert(g.sigma, g.ms, x, rnd() % (g.ms + 1));
		g.ms++;
		g.m++;
		reindex();
		eval_full(1);
		t = slot_tau(x);
		at = slot_pi(x);
		d = slot_sigma(x);
		relocate(g.tau, g_tpos[x], 1, t);
		relocate(g.pi, g_pipos[x], 1, at);
		relocate(g.sigma, g_spos[x], 1, d);
	}
	reindex();
	if (!accept(eval_full(0) - *cur))
	{
		g = save;
		reindex();
		eval_full(1);
		return (0);
	}
	*cur = eval_full(1);
	return (1);
}

#define WMAX 12
#define WINF (1 << 29)

static int	g_win = 0;
static int	g_dp[1 << WMAX][WMAX][3];
static int	g_par[1 << WMAX][WMAX][3];

static inline int	step_any(int phase, t_state *s, int e)
{
	int	p, q, d;

	if (phase)
		return (step(s, g_pipos[e], ac(e), g.m, g_n, &p, &q, &d));
	return (step0(s, e, &p, &q, &d));
}

/* the state after the window elements in set have moved, last one last */
static void	win_state(int phase, t_state *s, const int *el, int set, int last)
{
	int	i, e;

	for (i = 0; set >> i; i++)
	{
		if (!((set >> i) & 1))
			continue ;
		e = el[i];
		if (phase)
		{
			s->src[g_pipos[e] >> 6] &= ~(1ULL << (g_pipos[e] & 63));
			s->dst[ac(e) >> 6] |= 1ULL << (ac(e) & 63);
		}
		else
		{
			s->src[g_ipos[e] >> 6] &= ~(1ULL << (g_ipos[e] & 63));
			s->dst[g_pipos[e] >> 6] |= 1ULL << (g_pipos[e] & 63);
		}
		s->ssz--;
		s->dsz++;
	}
	e = el[last];
	if (s->ssz)
		s->stop = phase ? next_set(s->src, g_pipos[e], g.m)
			: next_set(s->src, g_ipos[e], g_n);
	s->dtop = phase ? ac(e) : g_pipos[e];
}

/* cost of everything after the window, from the state at its end */
static int	win_tail(int phase, t_state *s, int at)
{
	static t_state	s1;
	int				m = phase ? g.m : g.ms;

	if (at < m)
		return (step_any(phase, s, (phase ? g.tau : g.sigma)[at]));
	if (phase)
		return (tail_run(s, 0));
	start1(&s1, s);
	return (run1(&s1, NULL, NULL, 0) + tail_run(&s1, 0));
}

/* S6: the best order of w consecutive elements of tau or sigma, by dynamic
** programming over (moved set, last element, zigzag flag). Exact inside the
** window and for the step after it. Applies it and returns the gain */
static int	win_dp(int phase, int lo, int w)
{
	int		*ord = phase ? g.tau : g.sigma, *pre = phase ? g_pre1 : g_pre0;
	t_state	*ck = phase ? g_ck1 : g_ck0, s, t;
	int		m = phase ? g.m : g.ms, el[WMAX], full, set, l, z, y, c, old;
	int		best = WINF, bl = 0, bz = 0, i, nz;

	if (lo + w > m)
		w = m - lo;
	if (w < 3)
		return (0);
	for (i = 0; i < w; i++)
		el[i] = ord[lo + i];
	full = (1 << w) - 1;
	for (set = 0; set <= full; set++)
		for (l = 0; l < w; l++)
			for (z = 0; z < 3; z++)
				g_dp[set][l][z] = WINF;
	for (y = 0; y < w; y++)
	{
		t = ck[lo];
		c = step_any(phase, &t, el[y]);
		if (c < g_dp[1 << y][y][t.zz])
		{
			g_dp[1 << y][y][t.zz] = c;
			g_par[1 << y][y][t.zz] = -1;
		}
	}
	for (set = 1; set < full; set++)
		for (l = 0; l < w; l++)
			for (z = 0; z < 3; z++)
			{
				if (g_dp[set][l][z] >= WINF)
					continue ;
				s = ck[lo];
				win_state(phase, &s, el, set, l);
				s.zz = z;
				for (y = 0; y < w; y++)
				{
					if ((set >> y) & 1)
						continue ;
					t = s;
					c = g_dp[set][l][z] + step_any(phase, &t, el[y]);
					nz = t.zz;
					if (c < g_dp[set | (1 << y)][y][nz])
					{
						g_dp[set | (1 << y)][y][nz] = c;
						g_par[set | (1 << y)][y][nz] = l * 3 + z;
					}
				}
			}
	for (l = 0; l < w; l++)
		for (z = 0; z < 3; z++)
		{
			if (g_dp[full][l][z] >= WINF)
				continue ;
			s = ck[lo];
			win_state(phase, &s, el, full, l);
			s.zz = z;
			c = g_dp[full][l][z] + win_tail(phase, &s, lo + w);
			if (c < best)
			{
				best = c;
				bl = l;
				bz = z;
			}
		}
	s = ck[lo + w];
	old = pre[lo + w] - pre[lo] + win_tail(phase, &s, lo + w);
	if (best >= old)
		return (0);
	set = full;
	for (i = w - 1; i >= 0; i--)
	{
		ord[lo + i] = el[bl];
		c = g_par[set][bl][bz];
		set &= ~(1 << bl);
		bl = c / 3;
		bz = c % 3;
	}
	reindex();
	eval_full(1);
	return (old - best);
}

/* sweeps half-overlapping windows over both orders until a pass gains nothing */
static int	win_polish(int w)
{
	int	gain = 0, pass, lo, phase;

	do
	{
		pass = 0;
		for (phase = 1; phase >= 0; phase--)
		{
			if (!phase && g_npairs)
				continue ;
			eval_full(1);
			for (lo = 0; lo + 2 < (phase ? g.m : g.ms); lo += w / 2)
				pass += win_dp(phase, lo, w);
		}
		gain += pass;
	} while (pass > 0);
	return (gain);
}

static int	g_rate_def = 20;
static int	g_rate_x = 150;

/* cost of moving x in T1 and T2, from the prefix costs */
static int	moved_cost(int x)
{
	return (g_pre0[g_spos[x] + 1] - g_pre0[g_spos[x]]
		+ g_pre1[g_tpos[x] + 1] - g_pre1[g_tpos[x]]);
}

/* sweep key of a deferred element: its distance in g_ca from the top of a at
** the end of T2, given as an element */
static int	sweep_key(int y, int top_el)
{
	return ((g_ca[y] - g_ca[top_el] + g_n) % g_n);
}

/* defer one of the costliest moved elements: it leaves the three orders and
** joins T3 and T4, inserted where one forward sweep of a meets it, with b in
** the reverse order so that T4 pulls straight from the top of b */
static int	try_defer(int *cur)
{
	static t_gene	save;
	int				x, y, i, at, top_el, c, nxt;

	if (g.m < 3 || g_npairs)
		return (0);
	x = g.tau[rnd() % g.m];
	for (i = 0; i < 2; i++)
	{
		y = g.tau[rnd() % g.m];
		if (moved_cost(y) > moved_cost(x))
			x = y;
	}
	top_el = g_ac[g_ck1[g.m].dtop];
	save = g;
	arr_remove(g.sigma, g.ms, x);
	g.ms--;
	arr_remove(g.pi, g.m, x);
	arr_remove(g.tau, g.m, x);
	g.m--;
	g.defer[x] = 1;
	recompute_ca();
	for (at = 0; at < g.m2 && sweep_key(g.sig2[at], top_el)
		< sweep_key(x, top_el); at++)
		;
	nxt = at < g.m2 ? g.sig2[at] : -1;
	arr_insert(g.sig2, g.m2, x, at);
	if (nxt < 0)
	{
		arr_insert(g.pi2, g.m2, x, 0);
		arr_insert(g.tau2, g.m2, x, 0);
	}
	else
	{
		for (i = 0; g.pi2[i] != nxt; i++)
			;
		arr_insert(g.pi2, g.m2, x, i + 1);
		for (i = 0; g.tau2[i] != nxt; i++)
			;
		arr_insert(g.tau2, g.m2, x, i + 1);
	}
	g.m2++;
	reindex();
	c = eval_full(0);
	if (!accept(c - *cur))
	{
		g = save;
		reindex();
		return (0);
	}
	*cur = eval_full(1);
	return (1);
}

/* bring a deferred element back into T1 and T2, next to a rank neighbour in
** b and in the pull order, at a random point of the push order */
static int	try_undefer(int *cur)
{
	static t_gene	save;
	int				x, y, c;

	if (!g.m2)
		return (0);
	save = g;
	x = g.sig2[rnd() % g.m2];
	arr_remove(g.sig2, g.m2, x);
	arr_remove(g.pi2, g.m2, x);
	arr_remove(g.tau2, g.m2, x);
	g.m2--;
	g.defer[x] = 0;
	y = (x + g_n - 1) % g_n;
	arr_insert(g.sigma, g.ms, x, rnd() % (g.ms + 1));
	g.ms++;
	if (!g.keep[y] && !g.defer[y] && y != x)
	{
		arr_insert(g.pi, g.m, x, g_pipos[y] + 1);
		arr_insert(g.tau, g.m, x, g_tpos[y] + 1);
	}
	else
	{
		arr_insert(g.pi, g.m, x, rnd() % (g.m + 1));
		arr_insert(g.tau, g.m, x, rnd() % (g.m + 1));
	}
	g.m++;
	reindex();
	c = eval_full(0);
	if (!accept(c - *cur))
	{
		g = save;
		reindex();
		return (0);
	}
	*cur = eval_full(1);
	return (1);
}

/* reorder inside sig2, pi2 or tau2: only T3 and T4 change, so they are
** simulated alone from the saved state at the end of T2 */
static int	try_xlocal(int *cur)
{
	static int	save[MX];
	int			*arr, which = rnd() % 3, m = g.m2, from, to, c;

	if (m < 3)
		return (0);
	arr = which == 0 ? g.sig2 : which == 1 ? g.pi2 : g.tau2;
	memcpy(save, arr, sizeof(int) * m);
	from = rnd() % m;
	to = rnd() & 1 ? rnd() % m : from + (int)(rnd() % 7) - 3;
	if (to < 0)
		to = 0;
	if (to >= m)
		to = m - 1;
	if (to == from)
		return (0);
	relocate(arr, from, 1, to);
	reindex2();
	c = tail_run(&g_ck1[g.m], 0);
	if (!accept(c - g_finish))
	{
		memcpy(arr, save, sizeof(int) * m);
		reindex2();
		return (0);
	}
	*cur += c - g_finish;
	g_finish = c;
	return (1);
}

/* XSEED=k: defer at once every moved element whose push and pull cost k or
** more together, ordered for one forward sweep of a and pulled from the top
** of b. Returns the cost after */
static int	defer_expensive(int k)
{
	static int	pick[MX];
	int			np = 0, i, j, x, top_el;

	eval_full(1);
	top_el = g_ac[g_ck1[g.m].dtop];
	for (i = 0; i < g.m; i++)
		if (moved_cost(g.tau[i]) >= k)
			pick[np++] = g.tau[i];
	for (i = 0; i < np; i++)
	{
		x = pick[i];
		arr_remove(g.sigma, g.ms, x);
		g.ms--;
		arr_remove(g.pi, g.m, x);
		arr_remove(g.tau, g.m, x);
		g.m--;
		g.defer[x] = 1;
		g.sig2[g.m2++] = x;
	}
	recompute_ca();
	for (i = 1; i < g.m2; i++)
	{
		x = g.sig2[i];
		for (j = i - 1; j >= 0 && sweep_key(g.sig2[j], top_el)
			> sweep_key(x, top_el); j--)
			g.sig2[j + 1] = g.sig2[j];
		g.sig2[j + 1] = x;
	}
	/* T4 pulls in descending rank straight from the top of b, so b must hold
	** X in that order: T3 pays for sorting X while it fills b */
	np = 0;
	for (x = g_n - 1; x >= 0; x--)
		if (g.defer[x])
			g.pi2[np++] = x;
	for (i = 0; i < g.m2; i++)
		g.tau2[i] = g.pi2[i];
	reindex();
	return (eval_full(1));
}

static void	set_temp(double t)
{
	g_temp = t;
}

static t_stats	anneal(double secs, double t0, double t1, t_gene *best, int *bc)
{
	t_stats	st;
	double	start = now(), frac;
	int		cur = eval_full(1), r;

	st.iters = 0;
	st.acc = 0;
	*best = g;
	*bc = cur;
	while (1)
	{
		if ((st.iters & 1023) == 0)
		{
			frac = (now() - start) / secs;
			if (frac >= 1)
				break ;
			set_temp(t0 * pow(t1 / t0, frac));
		}
		st.iters++;
		draw_threshold();
		if (g_rate_rr && (int)(rnd() % 10000) < g_rate_rr)
		{
			st.acc += g_rr_fast ? try_rr_fast(&cur) : try_rr(&cur);
			if (cur < *bc)
			{
				*bc = cur;
				*best = g;
			}
			continue ;
		}
		if (g_four)
		{
			r = rnd() % 1000;
			if (r < g_rate_def || (r < g_rate_def + g_rate_x && g.m2 >= 3))
			{
				if (r >= g_rate_def)
					st.acc += try_xlocal(&cur);
				else if (g.m2 && (rnd() & 1))
					st.acc += try_undefer(&cur);
				else
					st.acc += try_defer(&cur);
				if (cur < *bc)
				{
					*bc = cur;
					*best = g;
				}
				continue ;
			}
		}
		r = rnd() % 1000;
		if (r < g_rate_pi)
			st.acc += try_pi(&cur);
		else if (r < g_rate_pi + g_rate_tog)
			st.acc += try_toggle(&cur);
		else if (r < g_rate_pi + g_rate_tog + g_rate_pair)
			st.acc += try_pair(rnd() & 1, &cur);
		else if (rnd() & 1)
			st.acc += try_local(1, &cur);
		else
			st.acc += try_local(0, &cur);
		if (cur < *bc)
		{
			*bc = cur;
			*best = g;
		}
	}
	st.secs = now() - start;
	return (st);
}

#define MAX_ARMS 512

typedef struct s_arm
{
	t_gene	g;
	int		cost;
	int		type;
	int		k;
	int		off;
	int		rev;
	double	gamma;
	int		ks;
}	t_arm;

static t_arm	g_arms[MAX_ARMS];
static int		g_cur_ks = 0;
static int		g_nks = 1;
static int		g_seed_best[3];
static int		g_ms_ready = 0;
static t_gene	g_ms[48];
static int		g_nms = 0;

static int	cmp_arm(const void *a, const void *b)
{
	return (((const t_arm *)a)->cost - ((const t_arm *)b)->cost);
}

static void	add_arm(int *na, int type, int k, int off, int rev, double gamma)
{
	if (*na >= MAX_ARMS)
		return ;
	if (type == 0)
		init_bands(k, off, rev, gamma);
	else if (type == 1)
		init_ring(k, off, rev, gamma);
	else
	{
		g = g_ms[k];
		reindex();
	}
	g_arms[*na].g = g;
	g_arms[*na].cost = eval_full(0);
	g_arms[*na].type = type;
	g_arms[*na].k = k;
	g_arms[*na].off = off;
	g_arms[*na].rev = rev;
	g_arms[*na].gamma = gamma;
	g_arms[*na].ks = g_cur_ks;
	if (!g_seed_best[type] || g_arms[*na].cost < g_seed_best[type])
		g_seed_best[type] = g_arms[*na].cost;
	(*na)++;
}

/* ARMS: 0 the original bands, 1 rings, 2 both. GAM: comma list of width
** profiles, the first one also used for one or two sweeps */
static int	build_arms_set(int na0)
{
	static const int	bands[] = {1, 2, 3, 4, 5, 6, 8};
	static const int	laps[] = {1, 2, 3, 4, 5, 6, 8, 10};
	double				gam[8] = {1.0};
	int					set = getenv("ARMS") ? atoi(getenv("ARMS")) : 0;
	int					ng = 1;
	int					i, o, r, gi, na = na0;
	char				*p = getenv("GAM");

	while (p && *p && ng <= 8)
	{
		gam[ng - 1] = atof(p);
		while (*p && *p != ',')
			p++;
		if (*p == ',' && ng < 8)
			ng++;
		if (*p)
			p++;
	}
	for (gi = 0; gi < ng; gi++)
	{
		if (set != 1)
			for (i = 0; i < 7; i++)
				for (o = 0; o < 4; o++)
					for (r = 0; r < 2; r++)
						if (gi == 0 || bands[i] >= 3)
							add_arm(&na, 0, bands[i], o * g_n / (4 * bands[i]),
								r, gam[gi]);
		if (set != 0)
			for (i = 0; i < 8; i++)
				for (o = 0; o < 4; o++)
					for (r = 0; r < 2; r++)
						if (gi == 0 || laps[i] >= 3)
							add_arm(&na, 1, laps[i], o * g_n / 4, r, gam[gi]);
	}
	for (i = 0; g_ms_ready && i < g_nms; i++)
		add_arm(&na, 2, i, 0, 0, 1.0);
	return (na);
}

/* every start once per keep set */
static int	build_arms(void)
{
	int	v, na = 0;

	for (v = 0; v < g_nks; v++)
	{
		load_ks(v, 1);
		g_cur_ks = v;
		na = build_arms_set(na);
	}
	return (na);
}

/* S4, multiscale: solve the sample of ranks off, off + step, ... with the
** same engine, then lift its shape to every rank. Each rank follows its
** sample representative: it leaves a in the representative's push sweep,
** sits next to it in b, and pulls are rebuilt greedily */
#define MS_MAX 8
#define MS_ARMS 48

static t_stats	race_arms(int na, double secs, double t0, double t1, t_arm **win);

static int	ms_rep(int x, int step, int off, int k, const char *kept)
{
	int	j = (x - off + step / 2) / step, d;

	if (x < off)
		j = 0;
	if (j >= k)
		j = k - 1;
	for (d = 0; d < k; d++)
	{
		if (j + d < k && !kept[j + d])
			return (j + d);
		if (j - d >= 0 && !kept[j - d])
			return (j - d);
	}
	return (-1);
}

static void	ms_solve(int step, int off, double secs)
{
	static int		init0[MX], ipos0[MX], sub[MX], lap[MX], side[MX];
	static int		key[MX], ord[MX], spos[MX], ppos[MX];
	static char		kept[MX];
	static t_gene	sg;
	t_arm			*win;
	static const int	nls[6] = {2, 3, 4, 6, 8, 12};
	int				n0 = g_n, k = 0, i, j, x, r, na, asc, ms, L;

	memcpy(init0, g_init, sizeof(int) * n0);
	memcpy(ipos0, g_ipos, sizeof(int) * n0);
	for (i = 0; i < n0; i++)
		if (g_init[i] % step == off % step && g_init[i] >= off)
			sub[k++] = (g_init[i] - off) / step;
	g_n = k;
	for (i = 0; i < k; i++)
	{
		g_init[i] = sub[i];
		g_ipos[sub[i]] = i;
	}
	init_keep();
	na = build_arms();
	race_arms(na, secs * 0.3, 1.2, 0.15, &win);
	g = win->g;
	reindex();
	anneal(secs * 0.7, 0.6, 0.15, &sg, &i);
	if (getenv("MSLOG"))
		fprintf(stderr, "sub cost %d m=%d\n", i, sg.m);
	ms = sg.m;
	for (j = 0; j < k; j++)
		kept[j] = sg.keep[j];
	for (i = 0; i < ms; i++)
	{
		spos[sg.sigma[i]] = i;
		ppos[sg.pi[i]] = i;
	}
	g_n = n0;
	memcpy(g_init, init0, sizeof(int) * n0);
	memcpy(g_ipos, ipos0, sizeof(int) * n0);
	init_keep();
	for (L = 0; L < 6 && g_nms < MS_ARMS; L++)
	{
		for (x = 0; x < n0; x++)
		{
			r = ms_rep(x, step, off, k, kept);
			/* the sample's push order, cut into nl sweeps of equal duration */
			lap[x] = r < 0 ? 0 : spos[r] * nls[L] / (ms > 0 ? ms : 1);
			/* cut b at the first sample element pushed: after it in pi means
			** the bottom of b, before it the top */
			side[x] = r >= 0 && ms > 0
				&& (ppos[r] - ppos[sg.sigma[0]] + ms) % ms < ms / 2;
			/* inside a group, follow the direction b takes at the representative */
			asc = r < 0 || ppos[r] + 1 >= ms || sg.pi[ppos[r] + 1] > r;
			j = x - (off + r * step);
			key[x] = (r < 0 ? 0 : ppos[r]) * (4 * step + 1) + 2 * step
				+ (asc ? j : -j);
		}
		init_laps(lap, side, nls[L]);
		memcpy(ord, g.pi, sizeof(int) * g.m);
		for (i = 1; i < g.m; i++)
		{
			x = ord[i];
			for (j = i - 1; j >= 0 && key[ord[j]] > key[x]; j--)
				ord[j + 1] = ord[j];
			ord[j + 1] = x;
		}
		if (getenv("MSPI"))
			memcpy(g.pi, ord, sizeof(int) * g.m);
		memcpy(g.tau, g.sigma, sizeof(int) * g.m);
		reindex();
		greedy_tau();
		reindex();
		if (getenv("MSLOG"))
			fprintf(stderr, "ms off=%d L=%d sub=%d lifted=%d\n", off, nls[L],
				sg.m, eval_full(0));
		g_ms[g_nms++] = g;
	}
}

/* successive halving: all arms share one cooling schedule, and each round
** gets the same total time, split among the arms still alive */
static t_stats	race_arms(int na, double secs, double t0, double t1, t_arm **win)
{
	static t_gene	best;
	t_stats			tot = {0, 0, 0}, st;
	int				rounds = 1, x = na, r, i, bc;
	double			ta, tb;

	while (x > 1)
	{
		x = (x + 1) / 2;
		rounds++;
	}
	for (r = 0; r < rounds; r++)
	{
		ta = t0 * pow(t1 / t0, (double)r / rounds);
		tb = t0 * pow(t1 / t0, (double)(r + 1) / rounds);
		for (i = 0; i < na; i++)
		{
			g = g_arms[i].g;
			load_ks(g_arms[i].ks, 0);
			reindex();
			st = anneal(secs / rounds / na, ta, tb, &best, &bc);
			g_arms[i].g = best;
			g_arms[i].cost = bc;
			tot.iters += st.iters;
			tot.acc += st.acc;
			tot.secs += st.secs;
		}
		qsort(g_arms, na, sizeof(t_arm), cmp_arm);
		na = (na + 1) / 2;
	}
	*win = &g_arms[0];
	return (tot);
}

int	main(int ac, char **av)
{
	static int		vals[MX];
	int				i, j, c, na;
	double			secs, t0, t1, frace;
	t_stats			st;
	t_arm			*win;

	if (ac < 5)
		return (1);
	secs = atof(av[1]);
	g_rng = strtoull(av[2], NULL, 10) * 2654435761ULL + 88172645463325252ULL;
	g_n = ac - 3;
	if (g_n > MX)
		return (1);
	for (i = 0; i < g_n; i++)
		vals[i] = atoi(av[3 + i]);
	for (i = 0; i < g_n; i++)
	{
		c = 0;
		for (j = 0; j < g_n; j++)
			c += vals[j] < vals[i];
		g_init[i] = c;
		g_ipos[c] = i;
	}
	g_four = getenv("FOUR") ? atoi(getenv("FOUR")) : 0;
	init_keep();
	save_ks(0);
	if (getenv("KEEPV") && atoi(getenv("KEEPV")))
	{
		g_force_pairs = 1;
		init_keep();
		save_ks(1);
		g_force_pairs = -1;
		g_nks = 2;
	}
	if (getenv("MS"))
	{
		int		nms = atoi(getenv("MS")) < MS_MAX ? atoi(getenv("MS")) : MS_MAX;
		int		stepm = g_n / 100 > 1 ? g_n / 100 : 2;
		double	msf = getenv("MSF") ? atof(getenv("MSF")) : 0.1;

		for (i = 0; i < nms; i++)
			ms_solve(stepm, i * stepm / nms, secs * msf / nms);
		secs -= secs * msf;
		g_ms_ready = 1;
	}
	na = build_arms();
	if (getenv("PI"))
		g_rate_pi = atoi(getenv("PI"));
	if (getenv("TOG"))
		g_rate_tog = atoi(getenv("TOG"));
	if (getenv("PAIR"))
		g_rate_pair = atoi(getenv("PAIR"));
	if (getenv("RR"))
		g_rate_rr = atoi(getenv("RR"));
	if (getenv("FOUR"))
		g_four = atoi(getenv("FOUR"));
	if (getenv("DEF"))
		g_rate_def = atoi(getenv("DEF"));
	if (getenv("XL"))
		g_rate_x = atoi(getenv("XL"));
	if (getenv("RRF"))
		g_rr_fast = atoi(getenv("RRF"));
	if (getenv("RRK"))
		g_rr_k = atoi(getenv("RRK")) < 15 ? atoi(getenv("RRK")) : 15;
	if (getenv("SLACK"))
		g_slack = atoi(getenv("SLACK"));
	if (getenv("TOURN"))
		g_tourn = atoi(getenv("TOURN"));
	if (getenv("LONG"))
		g_long = atoi(getenv("LONG"));
	for (i = 0; i < LOGU; i++)
		g_logu[i] = -log((i + 0.5) / LOGU);
	t0 = getenv("T0") ? atof(getenv("T0")) : 1.2;
	t1 = getenv("T1") ? atof(getenv("T1")) : 0.15;
	frace = getenv("FRACE") ? atof(getenv("FRACE")) : 0.3;
	st = race_arms(na, secs * frace, t0, t1, &win);
	g = win->g;
	load_ks(win->ks, 0);
	reindex();
	if (g_four && getenv("XSEED"))
	{
		int	before = eval_full(0), after = defer_expensive(atoi(getenv("XSEED")));

		fprintf(stderr, "xseed before=%d after=%d m2=%d T1=%d T2=%d tail=%d T3=%d", before,
			after, g.m2, g_pre0[g.ms], g_pre1[g.m], g_finish, g_t3cost);
		fputc(10, stderr);
	}
	if (frace < 1.0)
	{
		static t_gene	polished;
		int				pc;
		t_stats			st2;

		/* the race only picked a shape: reheat the winner and spend the rest on it */
		st2 = anneal(secs * (1.0 - frace),
				getenv("TR") ? atof(getenv("TR")) : 0.6, t1, &polished, &pc);
		st.iters += st2.iters;
		st.acc += st2.acc;
		st.secs += st2.secs;
		win->cost = pc;
		g = polished;
		reindex();
	}
	if (getenv("WIN"))
	{
		int	before = eval_full(0), wg;

		g_win = atoi(getenv("WIN")) < WMAX ? atoi(getenv("WIN")) : WMAX;
		wg = win_polish(g_win);
		fprintf(stderr, "win=%d before=%d gain=%d after=%d\n", g_win, before,
			wg, eval_full(0));
	}
	c = eval_full(0);
	fprintf(stderr, "arms=%d type=%d k=%d off=%d rev=%d gamma=%.1f seed_bands=%d seed_rings=%d best=%d recheck=%d iters=%ld rate=%.0f/s acc=%.3f keep=%d ks=%d pairs=%d m2=%d\n",
		na, win->type, win->k, win->off, win->rev, win->gamma, g_seed_best[0],
		g_seed_best[1], win->cost, c, st.iters,
		st.iters / st.secs, (double)st.acc / st.iters, g_n - g.m, win->ks,
		g_npairs, g.m2);
	if (getenv("DUMP"))
	{
		for (i = 0; i < g.m; i++)
			fprintf(stderr, "%d %d %d\n", g.sigma[i], g.pi[i], g.tau[i]);
	}
	g_out = stdout;
	emit_solution();
	fprintf(stderr, "emitted=%d\n", emit_final());
	return (0);
}
