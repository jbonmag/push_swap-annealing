#include "sa.h"

static int	add_rings(t_sa *x, t_ring *r, int na)
{
	int	o;

	o = -1;
	while (++o < 4)
	{
		r->centre = o * x->n / 4;
		r->desc = -1;
		while (++r->desc < 2)
		{
			sa_ring(x, r);
			x->arms[na].g = x->g;
			x->arms[na].cost = sa_eval(x, 0);
			x->order[na] = na;
			na++;
		}
	}
	return (na);
}

/*
** Rings of 1 to 10 sweeps with equal widths, and of 3 to 10 sweeps with wider
** first sweeps: SA_ARMS starts in all.
*/
void	sa_build_arms(t_sa *x)
{
	static const int	laps[8] = {1, 2, 3, 4, 5, 6, 8, 10};
	static const double	gammas[3] = {1.0, 0.6, 0.4};
	t_ring				r;
	int					i;
	int					na;

	na = 0;
	i = -1;
	while (++i < 24)
	{
		r.laps = laps[i % 8];
		r.gamma = gammas[i / 8];
		if (i < 8 || r.laps >= 3)
			na = add_rings(x, &r, na);
	}
}
