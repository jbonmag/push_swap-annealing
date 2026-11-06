#include "sa.h"

static void	clear_state(t_state *s)
{
	int	i;

	i = 0;
	while (i < SA_W)
	{
		s->src[i] = 0;
		s->dst[i] = 0;
		i++;
	}
	s->stop = 0;
	s->dtop = 0;
	s->ssz = 0;
	s->dsz = 0;
}

void	sa_start0(t_sa *x, t_state *s)
{
	int	i;

	clear_state(s);
	i = 0;
	while (i < x->n)
		sa_bit_set(s->src, i++);
	s->ssz = x->n;
}

void	sa_start1(t_sa *x, t_state *s, const t_state *end0)
{
	int	stop;
	int	dtop;
	int	i;

	stop = end0->dtop;
	dtop = x->init[end0->stop];
	clear_state(s);
	i = 0;
	while (i < x->g.m)
		sa_bit_set(s->src, i++);
	i = 0;
	while (i < x->n)
	{
		if (x->g.keep[i])
			sa_bit_set(s->dst, i);
		i++;
	}
	s->ssz = x->g.m;
	s->dsz = x->n - x->g.m;
	s->stop = stop;
	s->dtop = dtop;
}

/*
** Simulates a whole transfer. With save it also refreshes the checkpoints.
*/
int	sa_run(t_sa *x, t_state *s, int phase, int save)
{
	const int	*ord;
	int			i;
	int			c;

	ord = x->g.sigma;
	if (phase)
		ord = x->g.tau;
	c = 0;
	i = 0;
	while (i < x->g.m)
	{
		if (save)
		{
			x->ck[phase][i] = *s;
			x->pre[phase][i] = c;
		}
		c += sa_step_at(x, s, phase, ord[i]);
		i++;
	}
	if (save)
	{
		x->ck[phase][i] = *s;
		x->pre[phase][i] = c;
	}
	return (c);
}

int	sa_finish_cost(t_sa *x, const t_state *s)
{
	int	f;

	f = sa_cyc(s->dst, s->dtop, 0, x->n);
	if (f <= x->n - f)
		return (f);
	return (x->n - f);
}
