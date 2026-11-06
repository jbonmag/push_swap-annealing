#include "push_swap.h"

static int	is_valid_int(const char *s)
{
	int	i;

	i = 0;
	if (s[i] == '-' || s[i] == '+')
		i++;
	if (!s[i])
		return (0);
	while (s[i] >= '0' && s[i] <= '9')
		i++;
	if (s[i])
		return (0);
	return (1);
}

static void	fail(t_stack *a)
{
	free_stack(a);
	error_and_exit();
}

static void	check_duplicates(t_stack *a)
{
	t_node	*i;
	t_node	*j;

	i = a->top;
	while (i)
	{
		j = i->next;
		while (j)
		{
			if (i->value == j->value)
				fail(a);
			j = j->next;
		}
		i = i->next;
	}
}

static t_node	*new_node(t_stack *a, int val)
{
	t_node	*node;

	node = malloc(sizeof(t_node));
	if (!node)
		fail(a);
	node->value = val;
	node->rank = 0;
	node->next = NULL;
	return (node);
}

/*
** Every argument is checked before anything is allocated, so a bad one exits
** with nothing to free. argv[1] must be at the TOP of stack a: we iterate from
** argc-1 down to 1, prepending each node.
*/
void	parse_arguments(int argc, char **argv, t_stack *stack)
{
	t_node	*node;
	int		i;

	i = 0;
	while (++i < argc)
		if (!is_valid_int(argv[i]))
			error_and_exit();
	i = 0;
	while (++i < argc)
		ft_atoi(argv[i]);
	i = argc - 1;
	while (i >= 1)
	{
		node = new_node(stack, ft_atoi(argv[i]));
		node->next = stack->top;
		stack->top = node;
		stack->size++;
		i--;
	}
	check_duplicates(stack);
}
