#include "push_swap.h"

void	sol_init(t_solution *sol)
{
	sol->len = 0;
	sol->cap = SOL_CAP;
	sol->ops = malloc(sizeof(t_op) * sol->cap);
	if (!sol->ops)
		error_and_exit();
}

void	sol_free(t_solution *sol)
{
	free(sol->ops);
	free(sol);
}

/*
** Appends op, doubling the buffer when it is full.
*/
void	sol_push(t_solution *sol, t_op op)
{
	t_op	*grown;
	int		i;

	if (sol->len == sol->cap)
	{
		grown = malloc(sizeof(t_op) * sol->cap * 2);
		if (!grown)
			error_and_exit();
		i = -1;
		while (++i < sol->len)
			grown[i] = sol->ops[i];
		free(sol->ops);
		sol->ops = grown;
		sol->cap *= 2;
	}
	sol->ops[sol->len++] = op;
}

static void	write_op(t_op op)
{
	static const char	*names[] = {
		"", "sa\n", "sb\n", "ss\n", "pa\n", "pb\n",
		"ra\n", "rb\n", "rr\n", "rra\n", "rrb\n", "rrr\n"};
	static const int	lens[] = {
		0, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4};

	if (write(1, names[op], lens[op]) < 0)
		exit(EXIT_FAILURE);
}

void	flush_solution(t_solution *sol)
{
	int	i;

	i = 0;
	while (i < sol->len)
	{
		write_op(sol->ops[i]);
		i++;
	}
}
