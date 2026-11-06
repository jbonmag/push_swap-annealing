/*
** Probe: does mixing pushes and pulls pay? A solution is one event list where
** each moved element appears twice, leaving a and later returning. a holds
** elements waiting to leave next to the final sorted ones, so an arriving
** element may land anywhere between its sorted neighbours. Starts from a
** finished two-phase solution, which is one event list with no mixing.
**
** usage: interleave SECONDS SEED GENOME_FILE num...
** GENOME_FILE holds "sigma pi tau" lines as printed by engine with DUMP=1.
*/
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MX 512
#define W 8

static int		g_n;
static int		g_m;
static int		g_ev;
static int		g_init[MX];
static int		g_ipos[MX];
static char		g_keep[MX];
static int		g_events[MX * 2];
static int		g_pi[MX];
static int		g_pipos[MX];
static int		g_oldpre[MX * 2 + 1];
static int		g_limit;
static int		g_emit;
static uint64_t	g_rng;

static inline unsigned int	rnd(void)
{
	g_rng ^= g_rng << 13;
	g_rng ^= g_rng >> 7;
	g_rng ^= g_rng << 17;
	return ((unsigned int)(g_rng >> 20));
}

static void	emit(const char *op, int cnt)
{
	while (g_emit && cnt-- > 0)
		puts(op);
}

static inline int	count_range(const uint64_t *b, int lo, int hi)
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

static inline int	cyc(const uint64_t *b, int from, int to, int dom)
{
	if (from <= to)
		return (count_range(b, from, to));
	return (count_range(b, from, dom) + count_range(b, 0, to));
}

static inline int	next_set(const uint64_t *b, int c, int dom)
{
	int	i;

	for (i = 0; i < dom; i++)
	{
		int	at = (c + i) % dom;

		if ((b[at >> 6] >> (at & 63)) & 1)
			return (at);
	}
	return (c);
}

/* apply the cheapest rotation pair; fa/ba are a's forward and back distances,
** fb/bb are b's. returns the op count. *rot is a's net forward rotation */
static int	rotate_pair(int fa, int ba, int fb, int bb, int *rot)
{
	int	c0 = fa > fb ? fa : fb, c1 = ba > bb ? ba : bb, c2 = fa + bb, c3 = ba + fb;
	int	best = c0, d = 0, o;

	if (c1 < best) { best = c1; d = 1; }
	if (c2 < best) { best = c2; d = 2; }
	if (c3 < best) { best = c3; d = 3; }
	if (g_emit)
	{
		if (d == 0)
		{
			o = fa < fb ? fa : fb;
			emit("rr", o);
			emit("ra", fa - o);
			emit("rb", fb - o);
		}
		else if (d == 1)
		{
			o = ba < bb ? ba : bb;
			emit("rrr", o);
			emit("rra", ba - o);
			emit("rrb", bb - o);
		}
		else if (d == 2)
		{
			emit("ra", fa);
			emit("rrb", bb);
		}
		else
		{
			emit("rra", ba);
			emit("rb", fb);
		}
	}
	*rot = (d == 0 || d == 2) ? fa : -ba;
	return (best);
}

static void	rotate_arr(int *a, int an, int rot)
{
	static int	tmp[MX];

	if (an < 2 || rot == 0)
		return ;
	rot = ((rot % an) + an) % an;
	memcpy(tmp, a, sizeof(int) * rot);
	memmove(a, a + rot, sizeof(int) * (an - rot));
	memcpy(a + an - rot, tmp, sizeof(int) * rot);
}

/* returns the total cost, or -1 when it exceeds the running limit */
static int	simulate(int check, int *pre)
{
	static int	a[MX];
	static char	inf[MX];
	uint64_t	b[W] = {0};
	int			an = g_n, bsz = 0, btop = 0, i, c = 0, rot, f, r;

	memcpy(a, g_init, sizeof(int) * g_n);
	memcpy(inf, g_keep, g_n);
	for (i = 0; i < g_ev; i++)
	{
		int	e = g_events[i];

		if (pre)
			pre[i] = c;
		if (e < g_n)
		{
			int	p = 0, q = bsz ? cyc(b, btop, g_pipos[e], g_m) : 0;

			while (a[p] != e)
				p++;
			c += rotate_pair(p, an - p, q, bsz - q, &rot) + 1;
			rotate_arr(a, an, rot);
			memmove(a, a + 1, sizeof(int) * (an - 1));
			an--;
			emit("pb", 1);
			b[g_pipos[e] >> 6] |= 1ULL << (g_pipos[e] & 63);
			bsz++;
			btop = g_pipos[e];
		}
		else
		{
			int	y = e - g_n, pr = (y - 1 + g_n) % g_n, su = (y + 1) % g_n;
			int	ip = -1, is = -1, fa = 0, ba = 0;
			int	p = cyc(b, btop, g_pipos[y], g_m);

			while (!inf[pr])
				pr = (pr - 1 + g_n) % g_n;
			while (!inf[su])
				su = (su + 1) % g_n;
			for (f = 0; f < an && (ip < 0 || is < 0); f++)
			{
				if (a[f] == pr)
					ip = f;
				if (a[f] == su)
					is = f;
			}
			if (pr != su && is > ip)
			{
				fa = ip + 1;
				ba = an - is;
			}
			c += rotate_pair(fa, ba, p, bsz - p, &rot) + 1;
			rotate_arr(a, an, rot);
			memmove(a + 1, a, sizeof(int) * an);
			a[0] = y;
			an++;
			emit("pa", 1);
			inf[y] = 1;
			b[g_pipos[y] >> 6] &= ~(1ULL << (g_pipos[y] & 63));
			bsz--;
			if (bsz)
				btop = next_set(b, g_pipos[y], g_m);
		}
		if (check && c - g_oldpre[i + 1] > g_limit)
			return (-1);
	}
	r = 0;
	while (a[r] != 0)
		r++;
	if (pre)
		pre[g_ev] = c;
	if (r <= an - r)
	{
		emit("ra", r);
		return (c + r);
	}
	emit("rra", an - r);
	return (c + an - r);
}

static void	relocate(int *arr, int from, int to)
{
	int	v = arr[from];

	if (to < from)
		memmove(arr + to + 1, arr + to, sizeof(int) * (from - to));
	else
		memmove(arr + from, arr + from + 1, sizeof(int) * (to - from));
	arr[to] = v;
}

static int	valid_event_order(int moved)
{
	int	other = moved < g_n ? moved + g_n : moved - g_n, i, seen = 0;

	for (i = 0; i < g_ev; i++)
	{
		if (g_events[i] == moved || g_events[i] == other)
			if (seen++ == 0)
				return (g_events[i] < g_n);
	}
	return (1);
}

static inline int	pick_dist(int m)
{
	unsigned int	r = rnd() % 100;

	if (r < 60)
		return (1 + rnd() % 6);
	if (r < 92)
		return (1 + rnd() % 40);
	return (1 + rnd() % m);
}

static double	now(void)
{
	struct timespec	t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (t.tv_sec + t.tv_nsec * 1e-9);
}

int	main(int ac, char **av)
{
	static int	vals[MX], sigma[MX], tau[MX], best[MX * 2], bestpi[MX], tmp[MX * 2 + 1];
	double		secs = atof(av[1]), start, frac, T = 1, T0, T1;
	FILE		*f = fopen(av[3], "r");
	int			i, j, c, cur, bc, x, y, z;
	long		it = 0, acc = 0, mixed = 0;

	g_rng = strtoull(av[2], NULL, 10) * 2654435761ULL + 88172645463325252ULL;
	g_n = ac - 4;
	for (i = 0; i < g_n; i++)
		vals[i] = atoi(av[4 + i]);
	for (i = 0; i < g_n; i++)
	{
		c = 0;
		for (j = 0; j < g_n; j++)
			c += vals[j] < vals[i];
		g_init[i] = c;
		g_ipos[c] = i;
		g_keep[c] = 1;
	}
	while (f && fscanf(f, "%d %d %d", &x, &y, &z) == 3)
	{
		sigma[g_m] = x;
		g_pi[g_m] = y;
		tau[g_m] = z;
		g_pipos[y] = g_m;
		g_keep[x] = 0;
		g_m++;
	}
	for (i = 0; i < g_m; i++)
	{
		g_events[g_ev++] = sigma[i];
	}
	for (i = 0; i < g_m; i++)
		g_events[g_ev++] = tau[i] + g_n;
	T0 = getenv("T0") ? atof(getenv("T0")) : 0.6;
	T1 = getenv("T1") ? atof(getenv("T1")) : 0.12;
	cur = simulate(0, g_oldpre);
	bc = cur;
	memcpy(best, g_events, sizeof(int) * g_ev);
	memcpy(bestpi, g_pi, sizeof(int) * g_m);
	fprintf(stderr, "start=%d\n", cur);
	start = now();
	while (1)
	{
		if ((it & 255) == 0)
		{
			frac = (now() - start) / secs;
			if (frac >= 1)
				break ;
			T = T0 * pow(T1 / T0, frac);
		}
		it++;
		g_limit = (int)floor(-T * log((rnd() % 1048575 + 1) / 1048576.0)) + 8;
		if (rnd() % 100 < 88)
		{
			int	from = rnd() % g_ev, d = pick_dist(g_ev), to;
			int	e = g_events[from];

			to = (rnd() & 1) ? from + d : from - d;
			if (to < 0)
				to = 0;
			if (to >= g_ev)
				to = g_ev - 1;
			if (to == from)
				continue ;
			relocate(g_events, from, to);
			if (!valid_event_order(e))
			{
				relocate(g_events, to, from);
				continue ;
			}
			c = simulate(1, tmp);
			if (c < 0 || c - cur > g_limit - 8)
			{
				relocate(g_events, to, from);
				continue ;
			}
		}
		else
		{
			int	from = rnd() % g_m, d = pick_dist(g_m), to;

			to = (rnd() & 1) ? from + d : from - d;
			if (to < 0)
				to = 0;
			if (to >= g_m)
				to = g_m - 1;
			if (to == from)
				continue ;
			relocate(g_pi, from, to);
			for (i = from < to ? from : to; i <= (from < to ? to : from); i++)
				g_pipos[g_pi[i]] = i;
			c = simulate(1, tmp);
			if (c < 0 || c - cur > g_limit - 8)
			{
				relocate(g_pi, to, from);
				for (i = from < to ? from : to; i <= (from < to ? to : from); i++)
					g_pipos[g_pi[i]] = i;
				continue ;
			}
		}
		acc++;
		cur = c;
		memcpy(g_oldpre, tmp, sizeof(int) * (g_ev + 1));
		if (cur < bc)
		{
			bc = cur;
			memcpy(best, g_events, sizeof(int) * g_ev);
			memcpy(bestpi, g_pi, sizeof(int) * g_m);
		}
	}
	memcpy(g_events, best, sizeof(int) * g_ev);
	memcpy(g_pi, bestpi, sizeof(int) * g_m);
	for (i = 0; i < g_m; i++)
		g_pipos[g_pi[i]] = i;
	for (i = 1; i < g_ev; i++)
		mixed += (g_events[i - 1] >= g_n && g_events[i] < g_n);
	fprintf(stderr, "best=%d iters=%ld acc=%ld pulls_before_a_push=%ld\n", bc, it, acc, mixed);
	g_emit = 1;
	simulate(0, NULL);
	return (0);
}
