#include "push_swap.h"

void	error_and_exit(void)
{
	if (write(2, "Error\n", 6) < 0)
		exit(EXIT_FAILURE);
	exit(EXIT_FAILURE);
}

int	ft_atoi(const char *str)
{
	int		sign;
	long	res;

	sign = 1;
	res = 0;
	while (*str == ' ' || (*str >= 9 && *str <= 13))
		str++;
	if (*str == '-' || *str == '+')
	{
		if (*str == '-')
			sign = -1;
		str++;
	}
	while (*str >= '0' && *str <= '9')
	{
		res = res * 10 + (*str - '0');
		if (res > (long)INT_MAX + 1)
			error_and_exit();
		str++;
	}
	if (res * sign > INT_MAX || res * sign < INT_MIN)
		error_and_exit();
	return ((int)(res * sign));
}

void	free_stack(t_stack *stack)
{
	t_node	*curr;
	t_node	*tmp;

	curr = stack->top;
	while (curr)
	{
		tmp = curr;
		curr = curr->next;
		free(tmp);
	}
	stack->top = NULL;
	stack->size = 0;
}

int	stack_sorted(t_stack *a)
{
	t_node	*curr;

	curr = a->top;
	while (curr && curr->next)
	{
		if (curr->rank > curr->next->rank)
			return (0);
		curr = curr->next;
	}
	return (1);
}
