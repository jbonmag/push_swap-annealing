#include "push_swap.h"

static int	max_bits(int n)
{
	int	b;

	b = 0;
	while ((1 << b) < n)
		b++;
	return (b);
}

static void	pass_bit(t_stack *a, t_stack *b, t_solution *sol, int bit)
{
	int	n;
	int	i;

	n = a->size;
	i = 0;
	while (i < n)
	{
		if (((a->top->rank >> bit) & 1) == 0)
			op_pb(a, b, sol);
		else
			op_ra(a, sol);
		i++;
	}
	while (b->size > 0)
		op_pa(a, b, sol);
}

void	radix_sort(t_stack *a, t_stack *b, t_solution *sol)
{
	int	bits;
	int	i;

	if (a->size <= 1)
		return ;
	bits = max_bits(a->size);
	i = 0;
	while (i < bits)
	{
		pass_bit(a, b, sol, i);
		i++;
	}
}
