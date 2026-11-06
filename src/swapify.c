#include "sa.h"

static void	put3(t_op *ops, t_op a, t_op b, t_op c)
{
	ops[0] = a;
	ops[1] = b;
	ops[2] = c;
}

/*
** One rotation forward and one back around two pushes is a zigzag that a swap
** replaces with one op less. Returns 1 when ops holds a zigzag, writing the
** three ops that replace it to out.
*/
static int	zigzag(const t_op *o, t_op *out)
{
	if (o[0] == OP_RA && o[1] == OP_PB && o[2] == OP_RRA && o[3] == OP_PB)
		put3(out, OP_SA, OP_PB, OP_PB);
	else if (o[0] == OP_RB && o[1] == OP_PB && o[2] == OP_RRB && o[3] == OP_PB)
		put3(out, OP_PB, OP_SB, OP_PB);
	else if (o[0] == OP_RB && o[1] == OP_PA && o[2] == OP_RRB && o[3] == OP_PA)
		put3(out, OP_SB, OP_PA, OP_PA);
	else if (o[0] == OP_RA && o[1] == OP_PA && o[2] == OP_RRA && o[3] == OP_PA)
		put3(out, OP_PA, OP_SA, OP_PA);
	else
		return (0);
	return (1);
}

void	swapify(t_solution *sol)
{
	int	r;
	int	w;

	r = 0;
	w = 0;
	while (r < sol->len)
	{
		if (r + 3 < sol->len && zigzag(sol->ops + r, sol->ops + w))
		{
			w += 3;
			r += 4;
		}
		else
			sol->ops[w++] = sol->ops[r++];
	}
	sol->len = w;
}
