#include "sa.h"

/*
** Metropolis with the random draw made first: the trial knows the largest
** delta it may end with, so a clearly lost simulation can stop early.
*/
static void	draw_threshold(t_sa *x)
{
	x->d = (int)(x->temp * x->logu[sa_rnd(x) % SA_LOGU]);
	x->limit = x->d + SA_SLACK;
}

static void	one_move(t_sa *x)
{
	unsigned int	r;

	draw_threshold(x);
	r = sa_rnd(x) % 1000;
	if (r < 40)
		sa_try_pi(x);
	else if (r < 50)
		sa_try_toggle(x);
	else if (r < 90)
		sa_try_pair(x, sa_rnd(x) & 1);
	else if (sa_rnd(x) & 1)
		sa_try_local(x, 1);
	else
		sa_try_local(x, 0);
}

/*
** Runs one cooling from t0 to t1 on x->g for a given amount of simulated
** steps. Work replaces a clock, so the answer does not depend on the machine.
** Trials that simulate nothing are frequent when n is tiny, so the number of
** trials is capped by the same amount.
** The best solution found ends in x->best and x->bestc.
*/
void	sa_anneal(t_sa *x, long work, double t0, double t1)
{
	long	start;
	long	iters;
	double	frac;

	start = x->work;
	x->cur = sa_eval(x, 1);
	x->best = x->g;
	x->bestc = x->cur;
	x->temp = t0;
	iters = 0;
	while (x->work - start < work && iters < work)
	{
		if ((iters & 1023) == 0)
		{
			frac = (double)(x->work - start) / work;
			x->temp = sa_temp_at(t0, t1, frac);
		}
		iters++;
		one_move(x);
		if (x->cur < x->bestc)
		{
			x->bestc = x->cur;
			x->best = x->g;
		}
	}
}
