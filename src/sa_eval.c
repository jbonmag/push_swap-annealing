#include "sa.h"

int	sa_eval(t_sa *x, int save)
{
	int	c;
	int	f;

	sa_start0(x, &x->sa);
	c = sa_run(x, &x->sa, 0, save);
	sa_start1(x, &x->sb, &x->sa);
	c += sa_run(x, &x->sb, 1, save);
	f = sa_finish_cost(x, &x->sb);
	if (save)
		x->finish = f;
	return (c + f);
}
