#include "sa.h"

/*
** Simulates the steps of the reorder and one more, after which both orders
** are in the same state. A good reorder may cost more midway and recover, so
** this range never stops early. A change that reaches the end of the transfer
** also affects what follows it.
*/
static int	local_delta(t_sa *x, t_lm *l)
{
	int	i;
	int	d;

	l->end = l->hi + 1;
	if (l->end >= l->m)
		l->end = l->m - 1;
	x->sa = l->ck[l->lo];
	d = l->pre[l->lo] - l->pre[l->end + 1];
	i = l->lo;
	while (i <= l->end)
		d += sa_step_at(x, &x->sa, l->phase, l->ord[i++]);
	l->range = d;
	if (l->end < l->m - 1)
		return (d);
	if (l->phase)
		return (d + sa_finish_cost(x, &x->sa) - x->finish);
	sa_start1(x, &x->sb, &x->sa);
	d += sa_run(x, &x->sb, 1, 0);
	d += sa_finish_cost(x, &x->sb);
	return (d - x->pre[1][x->g.m] - x->finish);
}

static void	commit(t_sa *x, t_lm *l, int d)
{
	int	i;

	i = l->lo;
	while (i <= l->hi)
	{
		l->pos[l->ord[i]] = i;
		i++;
	}
	if (l->end == l->m - 1 && !l->phase)
	{
		x->cur = sa_eval(x, 1);
		return ;
	}
	sa_refresh(x, l);
	i = l->end + 2;
	while (i <= l->m)
		l->pre[i++] += l->range;
	if (l->end == l->m - 1)
		x->finish = sa_finish_cost(x, &l->ck[l->m]);
	x->cur += d;
}

static void	load_move(t_sa *x, t_lm *l, int phase)
{
	l->phase = phase;
	l->ord = x->g.sigma;
	l->pos = x->spos;
	if (phase)
	{
		l->ord = x->g.tau;
		l->pos = x->tpos;
	}
	l->ck = x->ck[phase];
	l->pre = x->pre[phase];
	l->m = x->g.m;
}

void	sa_try_local(t_sa *x, int phase)
{
	t_lm	l;
	int		r;
	int		d;

	load_move(x, &l, phase);
	if (l.m < 4)
		return ;
	r = sa_rnd(x) % 100;
	if (r < 12)
		sa_prep_reverse(x, &l);
	else if (!sa_prep_relocate(x, &l, r < 45))
		return ;
	d = local_delta(x, &l);
	if (sa_accept(x, d))
		commit(x, &l, d);
	else
		sa_icopy(l.ord + l.lo, x->save + l.lo, l.hi - l.lo + 1);
}
