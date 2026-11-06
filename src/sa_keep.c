#include "sa.h"

static void	load_rotation(t_sa *x, int off)
{
	int	i;

	i = 0;
	while (i < x->n)
	{
		x->val[i] = (x->init[(off + i) % x->n] - x->init[off] + x->n) % x->n;
		i++;
	}
}

static void	relax(t_sa *x, int i)
{
	int	j;

	x->dp[i] = 1;
	x->par[i] = -1;
	j = 0;
	while (j < i)
	{
		if (x->val[j] < x->val[i] && x->dp[j] + 1 > x->dp[i])
		{
			x->dp[i] = x->dp[j] + 1;
			x->par[i] = j;
		}
		j++;
	}
}

/*
** Longest increasing subsequence of the loaded rotation: returns its last
** index, and its length through len.
*/
static int	longest(t_sa *x, int *len)
{
	int	i;
	int	end;

	*len = 0;
	end = -1;
	i = 0;
	while (i < x->n)
	{
		relax(x, i);
		if (x->dp[i] > *len)
		{
			*len = x->dp[i];
			end = i;
		}
		i++;
	}
	return (end);
}

static void	mark_keep(t_sa *x, int off, int end)
{
	int	i;

	i = 0;
	while (i < SA_MAX)
		x->g.keep[i++] = 0;
	while (end >= 0)
	{
		x->g.keep[x->init[(off + end) % x->n]] = 1;
		end = x->par[end];
	}
}

/*
** Elements of a that never leave: the longest cyclically increasing
** subsequence, trying every rotation.
*/
void	sa_init_keep(t_sa *x)
{
	int	off;
	int	best;
	int	len;
	int	end;

	best = 0;
	off = 0;
	while (off < x->n)
	{
		load_rotation(x, off);
		end = longest(x, &len);
		if (len > best)
		{
			best = len;
			mark_keep(x, off, end);
		}
		off++;
	}
}
