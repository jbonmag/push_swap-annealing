#include "sa.h"

static int	max2(int a, int b)
{
	if (a > b)
		return (a);
	return (b);
}

/*
** Cheapest of the four rotation pairs: 0 both forward, 1 both back,
** 2 source forward and destination back, 3 the opposite.
*/
void	sa_choose_dir(t_rot *r)
{
	int	c;

	r->cost = max2(r->p, r->q);
	r->dir = 0;
	c = max2(r->ms - r->p, r->md - r->q);
	if (c < r->cost)
	{
		r->cost = c;
		r->dir = 1;
	}
	c = r->p + r->md - r->q;
	if (c < r->cost)
	{
		r->cost = c;
		r->dir = 2;
	}
	c = r->ms - r->p + r->q;
	if (c < r->cost)
	{
		r->cost = c;
		r->dir = 3;
	}
}

/*
** Moves one element between the two circular lists. c holds its source and
** destination coordinates, then the size of each coordinate domain.
*/
int	sa_step(t_state *s, const int *c, t_rot *r)
{
	r->p = sa_cyc(s->src, s->stop, c[0], c[2]);
	r->q = 0;
	if (s->dsz)
		r->q = sa_cyc(s->dst, s->dtop, c[1], c[3]);
	r->ms = s->ssz;
	r->md = s->dsz;
	sa_choose_dir(r);
	sa_bit_clear(s->src, c[0]);
	s->ssz--;
	if (s->ssz)
		s->stop = sa_next_set(s->src, c[0], c[2]);
	sa_bit_set(s->dst, c[1]);
	s->dsz++;
	s->dtop = c[1];
	return (r->cost + 1);
}

int	sa_step_at(t_sa *x, t_state *s, int phase, int e)
{
	int	c[4];

	x->work++;
	if (phase == 0)
	{
		c[0] = x->ipos[e];
		c[1] = x->pipos[e];
		c[2] = x->n;
		c[3] = x->g.m;
	}
	else
	{
		c[0] = x->pipos[e];
		c[1] = e;
		c[2] = x->g.m;
		c[3] = x->n;
	}
	return (sa_step(s, c, &x->rot));
}
