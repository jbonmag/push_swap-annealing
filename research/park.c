/* S8 probe: can "second trips" shorten the pull (T2) of an engine solution?
 *
 * Reads the input permutation (file in argv[1]) and the engine's operations
 * (stdin). Rebuilds the state after the last pb and the pull order tau, then
 * re-simulates T2 as a list of tasks:
 *   PULL x    rotate a to a valid gap for x and b to x (best of rr/rrr/mixed), pa
 *   PARK z    rotate b to z, pa: z waits in a, out of order
 *   UNPARK z  rotate a to z, pb: z is back in b, at a new place
 * A parked element is ignored when choosing gaps, so a pull may go on either
 * side of it. Greedy first-improvement search inserts PARK/UNPARK pairs
 * (park when z is at or passes b's top, unpark up to W tasks before z's pull)
 * and reports the T2 length before and after.
 *
 * build: cc -O2 -o park park.c
 * usage: park PERMFILE [W] < ops
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXN 1024
#define MAXT 4096

enum { PULL, PARK, UNPARK, EXIT };

typedef struct s_task { int type; int e; } t_task;

static int	g_n;
static int	g_a0[MAXN], g_la0, g_b0[MAXN], g_lb0;
static int	g_a[MAXN], g_b[MAXN], g_la, g_lb, g_tmp[MAXN];
static char	g_parked[MAXN];
/* trace of the last simulation: for each task, b's top before it and the
 * elements its b rotation passed */
static int	g_trace_on;
static int	g_pass_e[MAXT * 8], g_pass_s[MAXT * 8], g_npass;
static int	g_hist[8], g_hist_n = 8, g_hist_bad;
static int	g_apass_e[MAXT * 8], g_apass_s[MAXT * 8], g_napass;

static void	rot(int *s, int len, int r)
{
	if (len < 2 || r % len == 0)
		return ;
	r = ((r % len) + len) % len;
	memcpy(g_tmp, s, sizeof(int) * r);
	memmove(s, s + r, sizeof(int) * (len - r));
	memcpy(s + len - r, g_tmp, sizeof(int) * r);
}

static int	idx(int *s, int len, int e)
{
	for (int i = 0; i < len; i++)
		if (s[i] == e)
			return (i);
	return (-1);
}

static int	max2(int x, int y) { return (x > y ? x : y); }

/* rotation cost of reaching a-offset af (forward) and b-offset bf together;
 * applies the cheapest combination and returns its cost */
static int	combo(int af, int bf, int apply, int step)
{
	int	ar = g_la ? (g_la - af) % g_la : 0;
	int	br = g_lb ? (g_lb - bf) % g_lb : 0;
	int	c[4] = {max2(af, bf), max2(ar, br), af + br, ar + bf};
	int	best = 0;

	for (int k = 1; k < 4; k++)
		if (c[k] < c[best])
			best = k;
	if (apply)
	{
		int	bdir = (best == 0 || best == 3) ? 1 : -1;
		int	bsteps = bdir > 0 ? bf : br;
		if (g_trace_on && g_lb)
			for (int k = 0; k <= bsteps && g_npass < MAXT * 8; k++)
			{
				g_pass_e[g_npass] = g_b[((bdir * k) % g_lb + g_lb) % g_lb];
				g_pass_s[g_npass++] = step;
			}
		int	adir = (best == 0 || best == 2) ? 1 : -1;
		int	asteps = adir > 0 ? af : ar;
		if (g_trace_on && g_la)
			for (int k = 0; k <= asteps && g_napass < MAXT * 8; k++)
			{
				g_apass_e[g_napass] = g_a[((adir * k) % g_la + g_la) % g_la];
				g_apass_s[g_napass++] = step;
			}
		rot(g_a, g_la, af);
		rot(g_b, g_lb, bf);
	}
	return (c[best]);
}

static void	push(int *dst, int *ld, int *src, int *ls)
{
	int	e = src[0];

	memmove(src, src + 1, sizeof(int) * (*ls - 1));
	(*ls)--;
	memmove(dst + 1, dst, sizeof(int) * *ld);
	dst[0] = e;
	(*ld)++;
}

static int	pull_cost(int x, int apply, int step)
{
	int	bf = idx(g_b, g_lb, x);
	int	ip = -1, imax = -1, best = 1 << 30, bt = 0;

	for (int i = 0; i < g_la; i++)
	{
		int	v = g_a[i];
		if (g_parked[v])
			continue ;
		if (v < x && (ip < 0 || v > g_a[ip]))
			ip = i;
		if (imax < 0 || v > g_a[imax])
			imax = i;
	}
	if (ip < 0)
		ip = imax;
	for (int m = 0; m < max2(g_la, 1); m++)
	{
		int	t = ip < 0 ? m : (ip + 1 + m) % max2(g_la, 1);
		int	c = combo(t, bf, 0, step);
		if (c < best)
		{
			best = c;
			bt = t;
		}
		if (ip >= 0 && (g_la == 0 || !g_parked[g_a[t]]))
			break ;
	}
	if (apply)
		combo(bt, bf, 1, step);
	return (best);
}

static int	sim(t_task *t, int nt, int trace)
{
	int	ops = 0;

	memcpy(g_a, g_a0, sizeof(int) * g_la0);
	memcpy(g_b, g_b0, sizeof(int) * g_lb0);
	g_la = g_la0;
	g_lb = g_lb0;
	memset(g_parked, 0, g_n);
	g_trace_on = trace;
	g_npass = 0;
	g_napass = 0;
	for (int s = 0; s < nt; s++)
	{
		int	e = t[s].e;
		if (((t[s].type == UNPARK || t[s].type == EXIT) != (idx(g_a, g_la, e) >= 0))
			|| (t[s].type == EXIT && g_parked[e])
			|| (t[s].type == UNPARK && !g_parked[e]))
			return (1 << 29);
		if (t[s].type == PULL)
			ops += pull_cost(e, 1, s) + 1;
		else if (t[s].type == PARK)
			ops += combo(0, idx(g_b, g_lb, e), 1, s) + 1;
		else
			ops += combo(idx(g_a, g_la, e), 0, 1, s) + 1;
		if (t[s].type == UNPARK || t[s].type == EXIT)
			push(g_b, &g_lb, g_a, &g_la);
		else
			push(g_a, &g_la, g_b, &g_lb);
		g_parked[e] = t[s].type == PARK;
	}
	{
		int	i = idx(g_a, g_la, 0);
		ops += i < g_la - i ? i : g_la - i;
	}
	g_trace_on = 0;
	return (ops);
}

static int	cmp(const void *x, const void *y)
{
	return (**(const int **)x - **(const int **)y);
}

int	main(int ac, char **av)
{
	static int		v[MAXN], *pv[MAXN];
	static t_task	t[MAXT], u[MAXT];
	static char		buf[16];
	int				nt = 0, total = 0, t1 = 0, w = ac > 2 ? atoi(av[2]) : 12;
	FILE			*f = fopen(av[1], "r");

	while (fscanf(f, "%d", &v[g_n]) == 1)
		g_n++;
	fclose(f);
	for (int i = 0; i < g_n; i++)
		pv[i] = &v[i];
	qsort(pv, g_n, sizeof(int *), cmp);
	{
		static int	r[MAXN];
		for (int i = 0; i < g_n; i++)
			r[pv[i] - v] = i;
		memcpy(g_a, r, sizeof(int) * g_n);
	}
	g_la = g_n;
	g_lb = 0;
	/* replay: remember the state after the last pb */
	{
		static char	ops[MAXT * 4][4];
		int			no = 0, last = -1;
		while (scanf("%15s", buf) == 1)
		{
			strncpy(ops[no], buf, 3);
			if (!strcmp(buf, "pb"))
				last = no;
			no++;
		}
		total = no;
		for (int i = 0; i < no; i++)
		{
			char	*o = ops[i];
			if (!strcmp(o, "pb")) push(g_b, &g_lb, g_a, &g_la);
			else if (!strcmp(o, "pa"))
			{
				if (i > last)
					t[nt++] = (t_task){PULL, g_b[0]};
				push(g_a, &g_la, g_b, &g_lb);
			}
			else if (!strcmp(o, "ra")) rot(g_a, g_la, 1);
			else if (!strcmp(o, "rb")) rot(g_b, g_lb, 1);
			else if (!strcmp(o, "rr")) { rot(g_a, g_la, 1); rot(g_b, g_lb, 1); }
			else if (!strcmp(o, "rra")) rot(g_a, g_la, -1);
			else if (!strcmp(o, "rrb")) rot(g_b, g_lb, -1);
			else if (!strcmp(o, "rrr")) { rot(g_a, g_la, -1); rot(g_b, g_lb, -1); }
			else if (!strcmp(o, "sa") || !strcmp(o, "ss"))
			{ int x = g_a[0]; g_a[0] = g_a[1]; g_a[1] = x; }
			if (!strcmp(o, "sb") || !strcmp(o, "ss"))
			{ int x = g_b[0]; g_b[0] = g_b[1]; g_b[1] = x; }
			if (i == last)
			{
				memcpy(g_a0, g_a, sizeof(int) * g_la);
				memcpy(g_b0, g_b, sizeof(int) * g_lb);
				g_la0 = g_la;
				g_lb0 = g_lb;
				t1 = i + 1;
			}
		}
	}
	int	base = sim(t, nt, 0);
	printf("n=%d total=%d T1=%d T2 engine=%d resim=%d pulls=%d\n",
		g_n, total, t1, total - t1, base, nt);
	/* greedy search for PARK/UNPARK pairs */
	int	cur = base, pairs = 0, improved = 1;
	while (improved)
	{
		improved = 0;
		sim(t, nt, 1);
		int		np = g_npass;
		static int	pe[MAXT * 8], ps[MAXT * 8];
		memcpy(pe, g_pass_e, sizeof(int) * np);
		memcpy(ps, g_pass_s, sizeof(int) * np);
		for (int q = 0; q < np && !improved; q++)
		{
			int	z = pe[q], s = ps[q], k = -1;
			for (int j = s; j < nt; j++)
				if (t[j].type == PULL && t[j].e == z)
					k = j;
			if (k <= s)
				continue ;
			/* mode 0: unpark in [k-w, k], z's pull stays at k.
			 * mode 1: unpark in (s, s+w], z pulled right after it. */
			for (int mode = 0; mode < 2 && !improved; mode++)
			for (int j = mode ? s + 1 : max2(s + 1, k - w);
				j <= (mode ? s + w : k) && j <= nt && !improved; j++)
			{
				int	m = 0;
				for (int i = 0; i <= nt; i++)
				{
					if (i == s) u[m++] = (t_task){PARK, z};
					if (i == j) u[m++] = (t_task){UNPARK, z};
					if (i == j && mode) u[m++] = (t_task){PULL, z};
					if (i < nt && !(mode && i == k))
						u[m++] = t[i];
				}
				int	c = sim(u, m, 0);
				if (c < cur)
				{
					memcpy(t, u, sizeof(t_task) * m);
					nt = m;
					cur = c;
					pairs++;
					improved = 1;
				}
			}
		}
	}
	int	exits = 0;
	improved = 1;
	while (improved)
	{
		improved = 0;
		sim(t, nt, 1);
		int		np = g_napass;
		static int	pe[MAXT * 8], ps[MAXT * 8];
		memcpy(pe, g_apass_e, sizeof(int) * np);
		memcpy(ps, g_apass_s, sizeof(int) * np);
		for (int q = 0; q < np && !improved; q++)
		{
			int	y = pe[q], s = ps[q];
			for (int j = s + 1; j <= s + w && j <= nt && !improved; j++)
			{
				int	m = 0;
				for (int i = 0; i <= nt; i++)
				{
					if (i == s) u[m++] = (t_task){EXIT, y};
					if (i == j) u[m++] = (t_task){PULL, y};
					if (i < nt)
						u[m++] = t[i];
				}
				int	c = sim(u, m, 0);
				if (c - cur < g_hist_n && c >= cur)
					g_hist[c - cur]++;
				else if (c >= (1 << 29))
					g_hist_bad++;
				if (c < cur)
				{
					memcpy(t, u, sizeof(t_task) * m);
					nt = m;
					cur = c;
					exits++;
					improved = 1;
				}
			}
		}
	}
	printf("exits %d (worse by 0..7: %d %d %d %d %d %d %d %d, invalid %d): ", exits,
		g_hist[0], g_hist[1], g_hist[2], g_hist[3], g_hist[4], g_hist[5],
		g_hist[6], g_hist[7], g_hist_bad);
	printf("after %d pairs: T2=%d (gain %d)\n", pairs, cur, base - cur);
	return (0);
}
