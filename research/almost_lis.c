/* How many elements could stay in a if adjacent kept pairs may be inverted and
** fixed with sa? Compares the cyclic LIS with the same subsequence allowing
** disjoint inverted pairs. usage: almost_lis N RUNS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MX 1024

static int	g_n;

static int	best_rotation(const int *a, int with_pairs, int *npairs)
{
	static int	dp[MX + 1], cnt[MX + 1], pend[MX + 1][MX + 1], pcnt[MX + 1][MX + 1];
	int			p, q, v, x, best = 0, bestp = 0;

	/* dp[v]: longest kept sequence whose last value is v-1, dp[0]=0 is empty */
	memset(dp, -1, sizeof(int) * (g_n + 1));
	memset(cnt, 0, sizeof(int) * (g_n + 1));
	dp[0] = 0;
	for (p = 0; p <= g_n; p++)
		memset(pend[p], -1, sizeof(int) * (g_n + 1));
	for (p = 0; p < g_n; p++)
	{
		static int	pm[MX + 2], pc[MX + 2];

		for (v = 0; v <= g_n; v++)
			if (pend[p][v] > dp[v])
			{
				dp[v] = pend[p][v];
				cnt[v] = pcnt[p][v];
			}
		pm[0] = dp[0];
		pc[0] = cnt[0];
		for (v = 1; v <= g_n; v++)
		{
			pm[v] = pm[v - 1];
			pc[v] = pc[v - 1];
			if (dp[v] > pm[v])
			{
				pm[v] = dp[v];
				pc[v] = cnt[v];
			}
		}
		x = a[p];
		if (pm[x] + 1 > dp[x + 1])
		{
			dp[x + 1] = pm[x] + 1;
			cnt[x + 1] = pc[x];
		}
		if (with_pairs)
			for (q = p + 1; q < g_n; q++)
				if (a[q] < x && pm[a[q]] + 2 > pend[q + 1][x + 1])
				{
					pend[q + 1][x + 1] = pm[a[q]] + 2;
					pcnt[q + 1][x + 1] = pc[a[q]] + 1;
				}
	}
	for (v = 0; v <= g_n; v++)
	{
		if (pend[g_n][v] > dp[v])
		{
			dp[v] = pend[g_n][v];
			cnt[v] = pcnt[g_n][v];
		}
		if (dp[v] > best)
		{
			best = dp[v];
			bestp = cnt[v];
		}
	}
	*npairs = bestp;
	return (best);
}

int	main(int ac, char **av)
{
	static int	a[MX], r[MX];
	int			runs, i, j, t, np, n2;
	double		lis = 0, alm = 0, prs = 0;

	g_n = atoi(av[1]);
	runs = atoi(av[2]);
	srand(1);
	for (t = 0; t < runs; t++)
	{
		int	bl = 0, ba = 0, bp = 0, off;

		for (i = 0; i < g_n; i++)
			a[i] = i;
		for (i = g_n - 1; i > 0; i--)
		{
			j = rand() % (i + 1);
			int x = a[i]; a[i] = a[j]; a[j] = x;
		}
		for (off = 0; off < g_n; off += g_n > 200 ? g_n / 20 : 1)
		{
			for (i = 0; i < g_n; i++)
				r[i] = (a[(i + off) % g_n] - a[off] + g_n) % g_n;
			j = best_rotation(r, 0, &n2);
			if (j > bl)
				bl = j;
			j = best_rotation(r, 1, &np);
			if (j > ba)
			{
				ba = j;
				bp = np;
			}
		}
		lis += bl;
		alm += ba;
		prs += bp;
	}
	printf("n=%d lis=%.1f con_parejas=%.1f parejas=%.1f\n", g_n, lis / runs, alm / runs, prs / runs);
	return (0);
}
