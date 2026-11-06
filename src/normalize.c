#include "push_swap.h"

static int	*collect_values(t_stack *a)
{
	t_node	*curr;
	int		*arr;
	int		i;

	arr = malloc(sizeof(int) * a->size);
	if (!arr)
		error_and_exit();
	curr = a->top;
	i = 0;
	while (curr)
	{
		arr[i++] = curr->value;
		curr = curr->next;
	}
	return (arr);
}

static void	sort_int_arr(int *arr, int n)
{
	int	i;
	int	j;
	int	tmp;

	i = 1;
	while (i < n)
	{
		tmp = arr[i];
		j = i - 1;
		while (j >= 0 && arr[j] > tmp)
		{
			arr[j + 1] = arr[j];
			j--;
		}
		arr[j + 1] = tmp;
		i++;
	}
}

static int	find_rank(int *sorted, int n, int val)
{
	int	lo;
	int	hi;
	int	mid;

	lo = 0;
	hi = n - 1;
	while (lo <= hi)
	{
		mid = (lo + hi) / 2;
		if (sorted[mid] == val)
			return (mid);
		if (sorted[mid] < val)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return (-1);
}

/*
** Coordinate compression: assign rank 0..n-1 to each node.
** rank=0 is smallest value, rank=n-1 is largest.
*/
void	normalize(t_stack *a)
{
	int		*sorted;
	t_node	*curr;

	sorted = collect_values(a);
	sort_int_arr(sorted, a->size);
	curr = a->top;
	while (curr)
	{
		curr->rank = find_rank(sorted, a->size, curr->value);
		curr = curr->next;
	}
	free(sorted);
}
