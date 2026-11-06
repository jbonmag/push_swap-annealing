#include "sa.h"

void	sa_prep_reverse(t_sa *x, t_lm *l)
{
	l->lo = sa_rnd(x) % l->m;
	l->hi = l->lo + sa_pick_dist(x, l->m);
	if (l->hi >= l->m)
		l->hi = l->m - 1;
	sa_icopy(x->save + l->lo, l->ord + l->lo, l->hi - l->lo + 1);
	sa_reverse(l->ord, l->lo, l->hi);
}

static int	pick_local_target(t_sa *x, t_lm *l, int from, int guided)
{
	int	y;

	y = -1;
	if (guided && l->phase)
		y = sa_rank_neighbour(x, l->ord[from]);
	else if (guided)
		y = sa_pos_neighbour(x, l->ord[from]);
	if (y >= 0 && (l->pos[y] < from || l->pos[y] >= from + x->len))
		return (sa_guided_target(x, l->pos[y], l->m, from));
	return (sa_pick_target(x, l->m, from));
}

int	sa_prep_relocate(t_sa *x, t_lm *l, int guided)
{
	int	from;
	int	to;

	x->len = 1 + sa_rnd(x) % 3;
	from = sa_rnd(x) % (l->m - x->len + 1);
	to = pick_local_target(x, l, from, guided);
	if (to == from)
		return (0);
	l->lo = from;
	l->hi = to + x->len - 1;
	if (to < from)
	{
		l->lo = to;
		l->hi = from + x->len - 1;
	}
	sa_icopy(x->save + l->lo, l->ord + l->lo, l->hi - l->lo + 1);
	sa_relocate(x, l->ord, from, to);
	return (1);
}
