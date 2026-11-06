#include "sa.h"

void	solve(t_stack *a, t_stack *b, t_solution *sol)
{
	if (a->size <= 1)
		return ;
	normalize(a);
	if (stack_sorted(a))
		return ;
	if (a->size == 2)
		sort_two(a, sol);
	else if (a->size == 3)
		sort_three(a, sol);
	else if (a->size <= SA_MAX)
		sa_solve(a, sol);
	else
		radix_sort(a, b, sol);
}
