#include "sa.h"

static void	load_ranks(t_sa *x, t_stack *a)
{
	t_node	*c;
	int		i;

	x->n = a->size;
	c = a->top;
	i = 0;
	while (c)
	{
		x->init[i] = c->rank;
		x->ipos[c->rank] = i;
		c = c->next;
		i++;
	}
}

static long	work_for(int n)
{
	long	work;

	work = SA_BUDGET_N2 * n * n;
	if (work > SA_BUDGET)
		work = SA_BUDGET;
	return (work);
}

/*
** The race uses 30% of the work to choose a shape, then the winner is reheated
** and gets the rest.
*/
void	sa_solve(t_stack *a, t_solution *sol)
{
	t_sa	*x;
	long	work;
	double	t[2];

	x = malloc(sizeof(t_sa));
	if (!x)
		error_and_exit();
	x->work = 0;
	x->out = sol;
	load_ranks(x, a);
	sa_init_rng(x);
	sa_init_keep(x);
	work = work_for(x->n);
	sa_race(x, work * 3 / 10);
	t[0] = SA_TR;
	t[1] = SA_T1;
	sa_anneal(x, work - work * 3 / 10, t[0], t[1]);
	x->g = x->best;
	sa_reindex(x);
	sa_emit_solution(x);
	free(x);
	swapify(sol);
}
