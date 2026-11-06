#include "push_swap.h"

void	sort_two(t_stack *a, t_solution *sol)
{
	if (a->top->rank > a->top->next->rank)
		op_sa(a, sol);
}

/*
** All 6 cases for 3 elements by rank position (t=top, m=mid, b=bot):
** [max,min,mid] -> ra    [max,mid,min] -> sa+rra
** [mid,min,max] -> sa    [mid,max,min] -> rra
** [min,max,mid] -> rra+sa  [min,mid,max] -> sorted
*/
void	sort_three(t_stack *a, t_solution *sol)
{
	int	t;
	int	m;
	int	b;

	t = a->top->rank;
	m = a->top->next->rank;
	b = a->top->next->next->rank;
	if (t > m && t > b && m < b)
		op_ra(a, sol);
	else if (t > m && t > b && m > b)
	{
		op_sa(a, sol);
		op_rra(a, sol);
	}
	else if (m > t && m > b && t < b)
	{
		op_rra(a, sol);
		op_sa(a, sol);
	}
	else if (m > t && m > b && t > b)
		op_rra(a, sol);
	else if (b > t && b > m && t > m)
		op_sa(a, sol);
}
