#include "sa.h"

int	sa_pick_dist(t_sa *x, int m)
{
	unsigned int	r;

	r = sa_rnd(x) % 100;
	if (r < 60)
		return (1 + sa_rnd(x) % 8);
	if (r < 90)
		return (1 + sa_rnd(x) % 40);
	if (m < 1)
		m = 1;
	return (1 + sa_rnd(x) % m);
}

int	sa_pick_target(t_sa *x, int m, int from)
{
	int	d;
	int	to;

	d = sa_pick_dist(x, m);
	to = from - d;
	if (sa_rnd(x) & 1)
		to = from + d;
	if (to < 0)
		to = 0;
	if (to > m - x->len)
		to = m - x->len;
	return (to);
}

/*
** Where a block of len elements must land to sit beside position py.
*/
int	sa_guided_target(t_sa *x, int py, int m, int from)
{
	int	to;

	to = py + 1;
	if (py > from)
		to = py - x->len + 1;
	to -= sa_rnd(x) & 1;
	if (to < 0)
		to = 0;
	if (to > m - x->len)
		to = m - x->len;
	return (to);
}

int	sa_rank_neighbour(t_sa *x, int e)
{
	int	d;
	int	y;
	int	tries;

	d = 1 + sa_rnd(x) % 4;
	if (sa_rnd(x) & 1)
		d = -d;
	tries = 0;
	while (tries++ < 6)
	{
		y = (e + d + x->n) % x->n;
		if (!x->g.keep[y])
			return (y);
		if (d > 0)
			d++;
		else
			d--;
	}
	return (-1);
}

int	sa_pos_neighbour(t_sa *x, int e)
{
	int	d;
	int	y;
	int	tries;

	d = 1 + sa_rnd(x) % 4;
	if (sa_rnd(x) & 1)
		d = -d;
	tries = 0;
	while (tries++ < 6)
	{
		y = x->init[(x->ipos[e] + d + x->n) % x->n];
		if (!x->g.keep[y])
			return (y);
		if (d > 0)
			d++;
		else
			d--;
	}
	return (-1);
}
