#include "sa.h"

/*
** Walks a from its top in one direction looking for the closest element of
** sweep c that still has to leave. f is the distance of the first candidate.
*/
static void	scan(t_sa *x, int c, int dir, int f)
{
	int	i;
	int	at;
	int	e;

	i = 0;
	while (i < x->n)
	{
		at = (x->top + dir * (i + (dir < 0)) + x->n) % x->n;
		e = x->init[at];
		if (x->alive[at])
		{
			if (!x->g.keep[e] && x->lap[e] == c && f < x->pick_d)
			{
				x->pick_d = f;
				x->pick = at;
			}
			f++;
		}
		i++;
	}
}

static void	push_lap(t_sa *x, int c)
{
	int	e;

	x->pick = 0;
	while (x->pick >= 0)
	{
		x->pick = -1;
		x->pick_d = SA_ABORT;
		scan(x, c, 1, 0);
		scan(x, c, -1, 1);
		if (x->pick >= 0)
		{
			e = x->init[x->pick];
			x->g.sigma[x->g.m++] = e;
			if (x->side[e])
				x->bots[x->nb++] = e;
			else
				x->tops[x->nt++] = e;
			x->alive[x->pick] = 0;
			x->top = x->pick;
		}
	}
}

static void	build_pi(t_sa *x)
{
	int	i;

	i = 0;
	while (i < x->nt)
	{
		x->g.pi[i] = x->tops[x->nt - 1 - i];
		i++;
	}
	i = 0;
	while (i < x->nb)
	{
		x->g.pi[x->nt + i] = x->bots[i];
		i++;
	}
}

/*
** A starting solution from x->lap and x->side: lap says in which sweep of a an
** element leaves, side whether it goes to the bottom of b. Within a sweep the
** nearest pending element goes first.
*/
static void	sa_laps(t_sa *x, int nlaps)
{
	int	c;

	x->g.m = 0;
	x->nt = 0;
	x->nb = 0;
	x->top = 0;
	c = 0;
	while (c < x->n)
		x->alive[c++] = 1;
	c = -1;
	while (++c < nlaps)
		push_lap(x, c);
	build_pi(x);
	sa_icopy(x->g.tau, x->g.sigma, x->g.m);
	sa_reindex(x);
	sa_greedy_tau(x);
	sa_reindex(x);
}

/*
** A full ring around rank r->centre: sweep j pushes the ranks at about
** distance j from the centre on both sides, with wider first sweeps when
** gamma is below 1. desc sends the upper side to the top of b, so b ends
** descending and pulls chain without rotations.
*/
void	sa_ring(t_sa *x, const t_ring *r)
{
	int		e;
	int		sh;
	int		dist;
	double	rest;

	e = -1;
	while (++e < x->n)
	{
		sh = (e - r->centre + x->n) % x->n;
		dist = x->n - sh;
		if (sh < x->n / 2)
			dist = sh;
		rest = 1.0 - (double)dist / (x->n / 2 + 1);
		if (r->gamma != 1.0 && rest > 0.0)
			rest = sa_exp(r->gamma * sa_ln(rest));
		x->lap[e] = (int)(r->laps * (1.0 - rest));
		if (x->lap[e] >= r->laps)
			x->lap[e] = r->laps - 1;
		x->side[e] = (sh < x->n / 2) != r->desc;
	}
	sa_laps(x, r->laps);
}
