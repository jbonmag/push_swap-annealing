#include "sa.h"

/*
** Cost difference over a range of steps against the stored prefix costs, or
** SA_ABORT as soon as the trial is clearly lost.
*/
int	sa_bounded(t_sa *x, t_state *s, const t_span *sp)
{
	int	i;
	int	c;

	c = 0;
	i = sp->lo;
	while (i <= sp->hi)
	{
		c += sa_step_at(x, s, sp->phase, sp->ord[i]);
		if (sp->base + c - (sp->pre[i + 1] - sp->pre[sp->lo]) > x->limit)
			return (SA_ABORT);
		i++;
	}
	return (sp->base + c - (sp->pre[sp->hi + 1] - sp->pre[sp->lo]));
}

void	sa_refresh(t_sa *x, t_lm *l)
{
	t_state	s;
	int		i;
	int		c;

	s = l->ck[l->lo];
	c = l->pre[l->lo];
	i = l->lo;
	while (i <= l->end)
	{
		l->ck[i] = s;
		l->pre[i] = c;
		c += sa_step_at(x, &s, l->phase, l->ord[i]);
		i++;
	}
	l->ck[i] = s;
	l->pre[i] = c;
}

void	sa_reindex(t_sa *x)
{
	int	i;

	i = 0;
	while (i < x->g.m)
	{
		x->pipos[x->g.pi[i]] = i;
		x->spos[x->g.sigma[i]] = i;
		x->tpos[x->g.tau[i]] = i;
		i++;
	}
}

void	sa_icopy(int *dst, const int *src, int n)
{
	int	i;

	i = 0;
	while (i < n)
	{
		dst[i] = src[i];
		i++;
	}
}

int	sa_accept(t_sa *x, int delta)
{
	return (delta != SA_ABORT && delta <= x->d);
}
