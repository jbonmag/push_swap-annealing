#include "sa.h"

static int	cheapest_from(t_sa *x, const t_state *s, int i)
{
	t_rot	r;
	int		best;
	int		bj;
	int		e;

	best = SA_ABORT;
	bj = i;
	while (i < x->g.m)
	{
		e = x->g.tau[i];
		r.p = sa_cyc(s->src, s->stop, x->pipos[e], x->g.m);
		r.q = sa_cyc(s->dst, s->dtop, e, x->n);
		r.ms = s->ssz;
		r.md = s->dsz;
		sa_choose_dir(&r);
		if (r.cost < best)
		{
			best = r.cost;
			bj = i;
		}
		i++;
	}
	return (bj);
}

/*
** Orders the pulls: at each step, whichever element is cheapest right now.
*/
void	sa_greedy_tau(t_sa *x)
{
	int	i;
	int	j;
	int	e;

	sa_start0(x, &x->sa);
	sa_run(x, &x->sa, 0, 0);
	sa_start1(x, &x->sb, &x->sa);
	i = 0;
	while (i < x->g.m)
	{
		j = cheapest_from(x, &x->sb, i);
		e = x->g.tau[j];
		x->g.tau[j] = x->g.tau[i];
		x->g.tau[i] = e;
		sa_step_at(x, &x->sb, 1, e);
		i++;
	}
}
