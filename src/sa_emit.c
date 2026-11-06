#include "sa.h"

static void	push_ops(t_sa *x, t_op op, int cnt)
{
	while (cnt-- > 0)
		sol_push(x->out, op);
}

static void	emit_same(t_sa *x, t_op both, t_op *one, int *amount)
{
	int	o;

	o = amount[0];
	if (amount[1] < o)
		o = amount[1];
	push_ops(x, both, o);
	push_ops(x, one[0], amount[0] - o);
	push_ops(x, one[1], amount[1] - o);
}

static void	emit_rotations(t_sa *x, int phase, const t_rot *r)
{
	static const t_op	fwd[2][2] = {{OP_RA, OP_RB}, {OP_RB, OP_RA}};
	static const t_op	bck[2][2] = {{OP_RRA, OP_RRB}, {OP_RRB, OP_RRA}};
	t_op				one[2];
	int					am[2];

	am[0] = r->p;
	am[1] = r->q;
	one[0] = fwd[phase][0];
	one[1] = fwd[phase][1];
	if (r->dir == 0)
		emit_same(x, OP_RR, one, am);
	am[0] = r->ms - r->p;
	am[1] = r->md - r->q;
	one[0] = bck[phase][0];
	one[1] = bck[phase][1];
	if (r->dir == 1)
		emit_same(x, OP_RRR, one, am);
	if (r->dir == 2)
		push_ops(x, fwd[phase][0], r->p);
	if (r->dir == 2)
		push_ops(x, bck[phase][1], r->md - r->q);
	if (r->dir == 3)
		push_ops(x, bck[phase][0], r->ms - r->p);
	if (r->dir == 3)
		push_ops(x, fwd[phase][1], r->q);
}

static void	emit_phase(t_sa *x, t_state *s, int phase, const int *ord)
{
	int	i;

	i = 0;
	while (i < x->g.m)
	{
		sa_step_at(x, s, phase, ord[i]);
		emit_rotations(x, phase, &x->rot);
		if (phase == 0)
			sol_push(x->out, OP_PB);
		else
			sol_push(x->out, OP_PA);
		i++;
	}
}

void	sa_emit_solution(t_sa *x)
{
	int	f;

	sa_start0(x, &x->sa);
	emit_phase(x, &x->sa, 0, x->g.sigma);
	sa_start1(x, &x->sb, &x->sa);
	emit_phase(x, &x->sb, 1, x->g.tau);
	f = sa_cyc(x->sb.dst, x->sb.dtop, 0, x->n);
	if (f <= x->n - f)
		push_ops(x, OP_RA, f);
	else
		push_ops(x, OP_RRA, x->n - f);
}
