#include "push_swap.h"

int	main(int argc, char **argv)
{
	t_stack		a;
	t_stack		b;
	t_solution	*sol;

	if (argc < 2)
		return (0);
	a.top = NULL;
	a.size = 0;
	b.top = NULL;
	b.size = 0;
	parse_arguments(argc, argv, &a);
	sol = malloc(sizeof(t_solution));
	if (!sol)
	{
		free_stack(&a);
		error_and_exit();
	}
	sol_init(sol);
	solve(&a, &b, sol);
	flush_solution(sol);
	sol_free(sol);
	free_stack(&a);
	free_stack(&b);
	return (0);
}
