#include "sa.h"

/*
** Expresses a checkpointed b in the coordinates of the new pi: only positions
** lo..hi moved, and x->save holds who was at each of them.
*/
static void	relabel(t_sa *x, t_state *s, int lo, int hi)
{
	int	i;

	i = lo;
	while (i <= hi)
	{
		x->buf2[i] = sa_bit_get(s->dst, i);
		sa_bit_clear(s->dst, i);
		i++;
	}
	i = lo;
	while (i <= hi)
	{
		if (x->buf2[i])
			sa_bit_set(s->dst, x->pipos[x->save[i]]);
		i++;
	}
	if (s->dtop >= lo && s->dtop <= hi)
		s->dtop = x->pipos[x->save[s->dtop]];
}

static void	moved_bounds(t_sa *x, int to, int *first, int *last)
{
	int	i;

	*first = x->g.m;
	*last = 0;
	i = to;
	while (i < to + x->len)
	{
		if (x->spos[x->g.pi[i]] < *first)
			*first = x->spos[x->g.pi[i]];
		if (x->tpos[x->g.pi[i]] > *last)
			*last = x->tpos[x->g.pi[i]];
		i++;
	}
}

static int	pull_part(t_sa *x, int last, int base)
{
	t_span	sp;

	sa_start1(x, &x->sb, &x->sa);
	sp.ord = x->g.tau;
	sp.pre = x->pre[1];
	sp.lo = 0;
	sp.hi = last + 1;
	if (sp.hi >= x->g.m)
		sp.hi = x->g.m - 1;
	sp.phase = 1;
	sp.base = base;
	return (sa_bounded(x, &x->sb, &sp));
}

/*
** Only the moved elements change relative order. Pushes before the first of
** them cost the same, and so do pulls after the last of them, except the next
** one, whose starting top in b may differ.
*/
int	sa_pi_delta(t_sa *x, int to, int lo, int hi)
{
	t_span	sp;
	int		first;
	int		last;
	int		d;

	moved_bounds(x, to, &first, &last);
	x->sa = x->ck[0][first];
	relabel(x, &x->sa, lo, hi);
	sp.ord = x->g.sigma;
	sp.pre = x->pre[0];
	sp.lo = first;
	sp.hi = x->g.m - 1;
	sp.phase = 0;
	sp.base = 0;
	d = sa_bounded(x, &x->sa, &sp);
	if (d == SA_ABORT)
		return (d);
	return (pull_part(x, last, d));
}
