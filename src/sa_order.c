#include "sa.h"

/*
** Moves arr[from .. from + x->len) so that it starts at index to.
*/
void	sa_relocate(t_sa *x, int *arr, int from, int to)
{
	int	i;

	sa_icopy(x->buf, arr + from, x->len);
	i = from;
	while (i > to)
	{
		i--;
		arr[i + x->len] = arr[i];
	}
	i = from;
	while (i < to)
	{
		arr[i] = arr[i + x->len];
		i++;
	}
	sa_icopy(arr + to, x->buf, x->len);
}

void	sa_reverse(int *arr, int i, int j)
{
	int	t;

	while (i < j)
	{
		t = arr[i];
		arr[i] = arr[j];
		arr[j] = t;
		i++;
		j--;
	}
}

void	sa_arr_remove(int *a, int m, int v)
{
	int	i;

	i = 0;
	while (a[i] != v)
		i++;
	while (i + 1 < m)
	{
		a[i] = a[i + 1];
		i++;
	}
}

void	sa_arr_insert(int *a, int m, int v, int at)
{
	while (m > at)
	{
		a[m] = a[m - 1];
		m--;
	}
	a[at] = v;
}
