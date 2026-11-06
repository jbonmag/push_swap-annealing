#include "sa.h"

/*
** The draw order is fixed, so a given input always gets the same answer.
*/
void	sa_init_rng(t_sa *x)
{
	int	i;

	x->rng = 88172645463325252ULL + 2654435761ULL;
	i = 0;
	while (i < SA_LOGU)
	{
		x->logu[i] = -sa_ln((i + 0.5) / SA_LOGU);
		i++;
	}
}

t_uint	sa_rnd(t_sa *x)
{
	x->rng ^= x->rng << 13;
	x->rng ^= x->rng >> 7;
	x->rng ^= x->rng << 17;
	return ((t_uint)(x->rng >> 20));
}
