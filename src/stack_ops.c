#include "push_swap.h"

void	op_sa(t_stack *a, t_solution *sol)
{
	st_swap(a);
	sol_push(sol, OP_SA);
}

void	op_pa(t_stack *a, t_stack *b, t_solution *sol)
{
	st_push(b, a);
	sol_push(sol, OP_PA);
}

void	op_pb(t_stack *a, t_stack *b, t_solution *sol)
{
	st_push(a, b);
	sol_push(sol, OP_PB);
}

void	op_ra(t_stack *a, t_solution *sol)
{
	st_rotate(a);
	sol_push(sol, OP_RA);
}

void	op_rra(t_stack *a, t_solution *sol)
{
	st_rrotate(a);
	sol_push(sol, OP_RRA);
}
