#include "sa.h"

static void	place_beside(t_sa *x, int *arr, const int *pos, int *pair)
{
	int	from;
	int	py;
	int	to;

	from = pos[pair[0]];
	py = pos[pair[1]];
	x->len = 1;
	if (from < py)
		to = py - 1 + (sa_rnd(x) & 1);
	else
		to = py + (sa_rnd(x) & 1);
	if (to != from)
		sa_relocate(x, arr, from, to);
}

static int	pair_delta(t_sa *x)
{
	t_span	sp;
	int		d;

	sa_start0(x, &x->sa);
	sp.ord = x->g.sigma;
	sp.pre = x->pre[0];
	sp.lo = 0;
	sp.hi = x->g.m - 1;
	sp.phase = 0;
	sp.base = 0;
	d = sa_bounded(x, &x->sa, &sp);
	if (d == SA_ABORT)
		return (d);
	sa_start1(x, &x->sb, &x->sa);
	sp.ord = x->g.tau;
	sp.pre = x->pre[1];
	sp.phase = 1;
	sp.base = d;
	d = sa_bounded(x, &x->sb, &sp);
	if (d == SA_ABORT)
		return (d);
	return (d + sa_finish_cost(x, &x->sb) - x->finish);
}

static int	pick_pair(t_sa *x, int mode, int *pair)
{
	pair[0] = x->g.pi[sa_rnd(x) % x->g.m];
	if (mode)
		pair[1] = sa_pos_neighbour(x, pair[0]);
	else
		pair[1] = sa_rank_neighbour(x, pair[0]);
	return (pair[1] >= 0 && pair[1] != pair[0]);
}

/*
** A compound move: an element goes next to a neighbour both in pi and in one
** order, so the two consecutive transfers become cheap together. Mode 0 pairs
** rank neighbours in pi and tau, mode 1 pairs neighbours of a in pi and sigma.
*/
void	sa_try_pair(t_sa *x, int mode)
{
	int	*ord;
	int	*pos;
	int	pair[2];

	if (x->g.m < 4 || !pick_pair(x, mode, pair))
		return ;
	ord = x->g.tau;
	pos = x->tpos;
	if (mode)
		ord = x->g.sigma;
	if (mode)
		pos = x->spos;
	sa_icopy(x->save, x->g.pi, x->g.m);
	sa_icopy(x->buf2, ord, x->g.m);
	place_beside(x, x->g.pi, x->pipos, pair);
	place_beside(x, ord, pos, pair);
	sa_reindex(x);
	if (sa_accept(x, pair_delta(x)))
	{
		x->cur = sa_eval(x, 1);
		return ;
	}
	sa_icopy(x->g.pi, x->save, x->g.m);
	sa_icopy(ord, x->buf2, x->g.m);
	sa_reindex(x);
}
