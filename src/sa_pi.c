#include "sa.h"

static void	update_pos(t_sa *x, int lo, int hi)
{
	while (lo <= hi)
	{
		x->pipos[x->g.pi[lo]] = lo;
		lo++;
	}
}

static int	pick_pi_target(t_sa *x, int from)
{
	int	y;

	y = -1;
	if (sa_rnd(x) % 100 < 60)
		y = sa_rank_neighbour(x, x->g.pi[from]);
	if (y >= 0 && (x->pipos[y] < from || x->pipos[y] >= from + x->len))
		return (sa_guided_target(x, x->pipos[y], x->g.m, from));
	return (sa_pick_target(x, x->g.m, from));
}

/*
** A reorder of pi: the elements from lo to hi get new coordinates in b.
*/
static void	apply_pi(t_sa *x, int to, int lo, int hi)
{
	if (sa_accept(x, sa_pi_delta(x, to, lo, hi)))
	{
		x->cur = sa_eval(x, 1);
		return ;
	}
	sa_icopy(x->g.pi + lo, x->save + lo, hi - lo + 1);
	update_pos(x, lo, hi);
}

void	sa_try_pi(t_sa *x)
{
	int	from;
	int	to;
	int	lo;
	int	hi;

	if (x->g.m < 4)
		return ;
	x->len = 1 + sa_rnd(x) % 3;
	from = sa_rnd(x) % (x->g.m - x->len + 1);
	to = pick_pi_target(x, from);
	if (to == from)
		return ;
	lo = from;
	hi = to + x->len - 1;
	if (to < from)
	{
		lo = to;
		hi = from + x->len - 1;
	}
	sa_icopy(x->save + lo, x->g.pi + lo, hi - lo + 1);
	sa_relocate(x, x->g.pi, from, to);
	update_pos(x, lo, hi);
	apply_pi(x, to, lo, hi);
}
