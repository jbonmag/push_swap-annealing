/*
** Exact optimum of push_swap for small n, by BFS over the whole state graph.
**
** usage: bfs_exact N [MASK] [PHASED]
**   MASK    ops to forbid, one bit per op: 1 sa, 2 sb, 4 ss, 8 pb, 16 pa,
**           32 ra, 64 rb, 128 rr, 256 rra, 512 rrb, 1024 rrr
**   PHASED  1 restricts to two transfers: every pb comes before every pa.
**
** Without PHASED every op has its inverse in the op set, so the BFS runs from
** the sorted state. With PHASED a path from the sorted state that does all its
** pb before its pa, reversed and inverted, is a solution that does the same, so
** the BFS still runs from the sorted state with one extra bit: has a pa
** happened yet.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int	g_n;
static long	g_fact[13];

static long	rank_perm(const int *p)
{
	long	r = 0;
	int		i, j, c;

	for (i = 0; i < g_n; i++)
	{
		c = 0;
		for (j = i + 1; j < g_n; j++)
			c += p[j] < p[i];
		r += c * g_fact[g_n - 1 - i];
	}
	return (r);
}

static void	unrank_perm(long r, int *p)
{
	int	used[13] = {0};
	int	i, j, c;

	for (i = 0; i < g_n; i++)
	{
		c = r / g_fact[g_n - 1 - i];
		r %= g_fact[g_n - 1 - i];
		for (j = 0; j < g_n; j++)
			if (!used[j] && c-- == 0)
				break ;
		p[i] = j;
		used[j] = 1;
	}
}

static void	rot(int *x, int m, int rev)
{
	int	t, i;

	if (m < 2)
		return ;
	if (!rev)
	{
		t = x[0];
		for (i = 0; i + 1 < m; i++)
			x[i] = x[i + 1];
		x[m - 1] = t;
	}
	else
	{
		t = x[m - 1];
		for (i = m - 1; i > 0; i--)
			x[i] = x[i - 1];
		x[0] = t;
	}
}

static void	swp(int *x, int m)
{
	int	t;

	if (m < 2)
		return ;
	t = x[0];
	x[0] = x[1];
	x[1] = t;
}

/* p holds a top->bottom then b top->bottom; the first k entries are a */
static void	apply(int op, const int *p, int k, int *q, int *nk)
{
	int	i, m = g_n - k, t;

	memcpy(q, p, sizeof(int) * g_n);
	*nk = k;
	if (op == 0) swp(q, k);
	else if (op == 1) swp(q + k, m);
	else if (op == 2) { swp(q, k); swp(q + k, m); }
	else if (op == 3 && k > 0)
	{
		t = p[0];
		for (i = 0; i + 1 < k; i++)
			q[i] = p[i + 1];
		q[k - 1] = t;
		*nk = k - 1;
	}
	else if (op == 4 && m > 0)
	{
		t = p[k];
		for (i = k; i > 0; i--)
			q[i] = p[i - 1];
		q[0] = t;
		*nk = k + 1;
	}
	else if (op == 5) rot(q, k, 0);
	else if (op == 6) rot(q + k, m, 0);
	else if (op == 7) { rot(q, k, 0); rot(q + k, m, 0); }
	else if (op == 8) rot(q, k, 1);
	else if (op == 9) rot(q + k, m, 1);
	else if (op == 10) { rot(q, k, 1); rot(q + k, m, 1); }
}

int	main(int ac, char **av)
{
	long			tot, head = 0, tail = 0, i, s, t;
	unsigned char	*dist;
	unsigned int	*queue;
	int				mask, phased, p[13], q[13], k, nk, op, d, ph, nph, best, worst = 0;
	double			sum = 0;

	g_n = ac > 1 ? atoi(av[1]) : 8;
	mask = ac > 2 ? atoi(av[2]) : 0;
	phased = ac > 3 ? atoi(av[3]) : 0;
	g_fact[0] = 1;
	for (i = 1; i < 13; i++)
		g_fact[i] = g_fact[i - 1] * i;
	tot = g_fact[g_n] * (g_n + 1) * 2;
	dist = malloc(tot);
	queue = malloc(tot * sizeof(unsigned int));
	if (!dist || !queue)
		return (1);
	memset(dist, 255, tot);
	for (i = 0; i < g_n; i++)
		p[i] = i;
	s = (rank_perm(p) * (g_n + 1) + g_n) * 2;
	dist[s] = 0;
	queue[tail++] = (unsigned int)s;
	while (head < tail)
	{
		s = queue[head++];
		ph = s % 2;
		k = (s / 2) % (g_n + 1);
		d = dist[s];
		unrank_perm(s / 2 / (g_n + 1), p);
		for (op = 0; op < 11; op++)
		{
			if ((mask >> op) & 1)
				continue ;
			if (phased && op == 3 && ph)
				continue ;
			nph = ph || (phased && op == 4);
			apply(op, p, k, q, &nk);
			t = (rank_perm(q) * (g_n + 1) + nk) * 2 + nph;
			if (dist[t] == 255)
			{
				dist[t] = d + 1;
				queue[tail++] = (unsigned int)t;
			}
		}
	}
	/* DUMP=path: exact cost-to-go of every state as one byte, indexed by
	** rank_perm * (N + 1) + k, where k is the size of a. Undirected graph only */
	if (getenv("DUMP") && !phased)
	{
		FILE	*f = fopen(getenv("DUMP"), "wb");

		for (i = 0; f && i < g_fact[g_n] * (g_n + 1); i++)
			fputc(dist[i * 2], f);
		if (f)
			fclose(f);
	}
	/* WALK: follow one optimal path per random start, preferring rotations,
	** then pa, then swaps, then pb, and count pushes. Undirected graph only */
	if (getenv("WALK") && !phased)
	{
		static const int	pref[11] = {5, 6, 7, 8, 9, 10, 4, 0, 1, 2, 3};
		double				pushes = 0;
		int					run, j;

		srand(3);
		for (run = 0; run < 3000; run++)
		{
			for (j = 0; j < g_n; j++)
				p[j] = j;
			for (j = g_n - 1; j > 0; j--)
			{
				int	r = rand() % (j + 1), tmp = p[j];

				p[j] = p[r];
				p[r] = tmp;
			}
			k = g_n;
			s = (rank_perm(p) * (g_n + 1) + k) * 2;
			while (dist[s] > 0)
			{
				for (j = 0; j < 11; j++)
				{
					op = pref[j];
					if ((mask >> op) & 1)
						continue ;
					apply(op, p, k, q, &nk);
					t = (rank_perm(q) * (g_n + 1) + nk) * 2;
					if (dist[t] == dist[s] - 1)
						break ;
				}
				pushes += op == 3;
				memcpy(p, q, sizeof(int) * g_n);
				k = nk;
				s = t;
			}
		}
		printf("walk: pb per optimal solution %.2f\n", pushes / 3000);
	}
	/* SWAPS: follow one optimal path per random start, avoiding swaps when a
	** tie allows it, and tally each swap by phase and by the ops around it.
	** Phase: 0 before any pb, 1 after a pb and before any pa, 2 after a pa */
	if (getenv("SWAPS") && !phased)
	{
		static const int	pref[11] = {5, 6, 7, 8, 9, 10, 4, 3, 0, 1, 2};
		static const char	*nm[12] = {"sa", "sb", "ss", "pb", "pa", "ra", "rb",
			"rr", "rra", "rrb", "rrr", "-"};
		static long			ctx[3][3][12][12];
		static int			path[64];
		long				tot = 0, ph[3][3] = {{0}};
		int					run, j, len, phs, sw, a2, b2;

		srand(5);
		for (run = 0; run < 20000; run++)
		{
			for (j = 0; j < g_n; j++)
				p[j] = j;
			for (j = g_n - 1; j > 0; j--)
			{
				int	r = rand() % (j + 1), tmp = p[j];

				p[j] = p[r];
				p[r] = tmp;
			}
			k = g_n;
			s = (rank_perm(p) * (g_n + 1) + k) * 2;
			len = 0;
			while (dist[s] > 0)
			{
				for (j = 0; j < 11; j++)
				{
					op = pref[j];
					if ((mask >> op) & 1)
						continue ;
					apply(op, p, k, q, &nk);
					t = (rank_perm(q) * (g_n + 1) + nk) * 2;
					if (dist[t] == dist[s] - 1)
						break ;
				}
				path[len++] = op;
				memcpy(p, q, sizeof(int) * g_n);
				k = nk;
				s = t;
			}
			phs = 0;
			for (j = 0; j < len; j++)
			{
				if (path[j] == 3 && phs == 0)
					phs = 1;
				if (path[j] == 4)
					phs = 2;
				if (path[j] > 2)
					continue ;
				sw = path[j];
				a2 = j > 0 ? path[j - 1] : 11;
				b2 = j + 1 < len ? path[j + 1] : 11;
				ctx[phs][sw][a2][b2]++;
				ph[phs][sw]++;
				tot++;
			}
		}
		printf("swaps per optimal solution %.3f\n", tot / 20000.0);
		for (phs = 0; phs < 3; phs++)
			for (sw = 0; sw < 3; sw++)
				if (ph[phs][sw])
					printf("phase %d %s: %.3f per solution\n", phs, nm[sw],
						ph[phs][sw] / 20000.0);
		for (phs = 0; phs < 3; phs++)
			for (sw = 0; sw < 3; sw++)
				for (a2 = 0; a2 < 12; a2++)
					for (b2 = 0; b2 < 12; b2++)
						if (ctx[phs][sw][a2][b2] * 100 >= tot)
							printf("  ph%d %s %s %s: %.1f%%\n", phs, nm[a2],
								nm[sw], nm[b2],
								100.0 * ctx[phs][sw][a2][b2] / tot);
	}
	/* every permutation of a full stack a, with b empty, in either phase */
	for (i = 0; i < g_fact[g_n]; i++)
	{
		s = (i * (g_n + 1) + g_n) * 2;
		best = dist[s] < dist[s + 1] ? dist[s] : dist[s + 1];
		if (best == 255)
			return (2);
		sum += best;
		if (best > worst)
			worst = best;
	}
	printf("n=%d mask=%d phased=%d avg_opt=%.3f worst=%d\n",
		g_n, mask, phased, sum / g_fact[g_n], worst);
	return (0);
}
