#include "sa.h"

/*
** The kept elements must stay cyclically increasing in the initial order.
*/
static int	keep_ok(t_sa *x)
{
	int	i;
	int	prev;
	int	first;
	int	drops;

	prev = -1;
	first = -1;
	drops = 0;
	i = -1;
	while (++i < x->n)
	{
		if (x->g.keep[x->init[i]])
		{
			if (first < 0)
				first = x->init[i];
			if (prev >= 0 && x->init[i] < prev)
				drops++;
			prev = x->init[i];
		}
	}
	if (first < 0)
		return (0);
	if (first < prev)
		drops++;
	return (drops <= 1);
}

static void	release(t_sa *x, int e)
{
	sa_arr_insert(x->g.sigma, x->g.m, e, sa_rnd(x) % (x->g.m + 1));
	sa_arr_insert(x->g.pi, x->g.m, e, sa_rnd(x) % (x->g.m + 1));
	sa_arr_insert(x->g.tau, x->g.m, e, sa_rnd(x) % (x->g.m + 1));
	x->g.m++;
}

static void	retain(t_sa *x, int e)
{
	sa_arr_remove(x->g.sigma, x->g.m, e);
	sa_arr_remove(x->g.pi, x->g.m, e);
	sa_arr_remove(x->g.tau, x->g.m, e);
	x->g.m--;
}

/*
** Moves one element into or out of the set that never leaves a.
*/
void	sa_try_toggle(t_sa *x)
{
	int	e;

	e = sa_rnd(x) % x->n;
	if ((!x->g.keep[e] && x->g.m < 2) || x->g.m >= SA_MAX - 1)
		return ;
	x->saved = x->g;
	x->g.keep[e] = !x->g.keep[e];
	if (!keep_ok(x))
	{
		x->g.keep[e] = !x->g.keep[e];
		return ;
	}
	if (x->g.keep[e])
		retain(x, e);
	else
		release(x, e);
	sa_reindex(x);
	if (sa_accept(x, sa_eval(x, 0) - x->cur))
	{
		x->cur = sa_eval(x, 1);
		return ;
	}
	x->g = x->saved;
	sa_reindex(x);
}
