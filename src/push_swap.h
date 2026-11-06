#ifndef PUSH_SWAP_H
# define PUSH_SWAP_H

# include <stdlib.h>
# include <unistd.h>
# include <limits.h>

# define SOL_CAP 4096

typedef struct s_node
{
	int				value;
	int				rank;
	struct s_node	*next;
}	t_node;

typedef struct s_stack
{
	t_node	*top;
	int		size;
}	t_stack;

typedef enum e_op
{
	OP_NONE,
	OP_SA,
	OP_SB,
	OP_SS,
	OP_PA,
	OP_PB,
	OP_RA,
	OP_RB,
	OP_RR,
	OP_RRA,
	OP_RRB,
	OP_RRR
}	t_op;

typedef struct s_solution
{
	t_op	*ops;
	int		len;
	int		cap;
}	t_solution;

/* utils.c */
void	error_and_exit(void);
int		ft_atoi(const char *str);
void	free_stack(t_stack *stack);
int		stack_sorted(t_stack *a);

/* solution.c */
void	sol_init(t_solution *sol);
void	sol_push(t_solution *sol, t_op op);
void	flush_solution(t_solution *sol);
void	sol_free(t_solution *sol);

/* stack_core.c */
void	st_swap(t_stack *s);
void	st_push(t_stack *src, t_stack *dst);
void	st_rotate(t_stack *s);
void	st_rrotate(t_stack *s);

/* stack_ops.c */
void	op_sa(t_stack *a, t_solution *sol);
void	op_pa(t_stack *a, t_stack *b, t_solution *sol);
void	op_pb(t_stack *a, t_stack *b, t_solution *sol);
void	op_ra(t_stack *a, t_solution *sol);
void	op_rra(t_stack *a, t_solution *sol);

/* parse.c */
void	parse_arguments(int argc, char **argv, t_stack *stack);

/* normalize.c */
void	normalize(t_stack *a);

/* small_sort.c */
void	sort_two(t_stack *a, t_solution *sol);
void	sort_three(t_stack *a, t_solution *sol);

/* radix_sort.c */
void	radix_sort(t_stack *a, t_stack *b, t_solution *sol);

/* solve.c */
void	solve(t_stack *a, t_stack *b, t_solution *sol);

#endif
