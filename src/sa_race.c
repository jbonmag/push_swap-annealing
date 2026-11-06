#include "sa.h"

static void	sort_arms(t_sa *x, int na)
{
	int	i;
	int	j;
	int	v;

	i = 1;
	while (i < na)
	{
		v = x->order[i];
		j = i - 1;
		while (j >= 0 && x->arms[x->order[j]].cost > x->arms[v].cost)
		{
			x->order[j + 1] = x->order[j];
			j--;
		}
		x->order[j + 1] = v;
		i++;
	}
}

static int	count_rounds(int na)
{
	int	rounds;

	rounds = 1;
	while (na > 1)
	{
		na = (na + 1) / 2;
		rounds++;
	}
	return (rounds);
}

static void	run_arm(t_sa *x, t_arm *arm, long work, const double *t)
{
	x->g = arm->g;
	sa_reindex(x);
	sa_anneal(x, work, t[0], t[1]);
	arm->g = x->best;
	arm->cost = x->bestc;
}

/*
** Successive halving over the starting shapes of b, because annealing cannot
** change a shape on its own. All arms share one cooling schedule, and each
** round gets the same work, split among the arms still alive. Leaves the
** winner in x->g.
*/
void	sa_race(t_sa *x, long work)
{
	double	t[2];
	int		rounds;
	int		r;
	int		i;
	int		na;

	sa_build_arms(x);
	na = SA_ARMS;
	rounds = count_rounds(na);
	r = -1;
	while (++r < rounds)
	{
		t[0] = sa_temp_at(SA_T0, SA_T1, (double)r / rounds);
		t[1] = sa_temp_at(SA_T0, SA_T1, (double)(r + 1) / rounds);
		i = -1;
		while (++i < na)
			run_arm(x, &x->arms[x->order[i]], work / rounds / na, t);
		sort_arms(x, na);
		na = (na + 1) / 2;
	}
	x->g = x->arms[x->order[0]].g;
	sa_reindex(x);
}
