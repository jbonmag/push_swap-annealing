#ifndef SA_H
# define SA_H

# include "push_swap.h"

# define SA_W 8
# define SA_MAX 512
# define SA_LOGU 4096
# define SA_ABORT 536870912
# define SA_SLACK 8
# define SA_ARMS 160
# define SA_T0 1.2
# define SA_T1 0.15
# define SA_TR 0.6
# define SA_BUDGET 40000000L
# define SA_BUDGET_N2 90000L

typedef unsigned long long	t_u64;
typedef unsigned int		t_uint;

typedef struct s_state
{
	t_u64	src[SA_W];
	t_u64	dst[SA_W];
	int		stop;
	int		dtop;
	int		ssz;
	int		dsz;
}	t_state;

typedef struct s_rot
{
	int	p;
	int	ms;
	int	q;
	int	md;
	int	dir;
	int	cost;
}	t_rot;

typedef struct s_gene
{
	int	keep[SA_MAX];
	int	sigma[SA_MAX];
	int	pi[SA_MAX];
	int	tau[SA_MAX];
	int	m;
}	t_gene;

typedef struct s_arm
{
	t_gene	g;
	int		cost;
}	t_arm;

typedef struct s_ring
{
	int		laps;
	int		centre;
	int		desc;
	double	gamma;
}	t_ring;

typedef struct s_span
{
	const int	*ord;
	const int	*pre;
	int			lo;
	int			hi;
	int			phase;
	int			base;
}	t_span;

typedef struct s_lm
{
	int		phase;
	int		*ord;
	int		*pos;
	t_state	*ck;
	int		*pre;
	int		m;
	int		lo;
	int		hi;
	int		end;
	int		range;
}	t_lm;

typedef struct s_sa
{
	int			n;
	int			init[SA_MAX];
	int			ipos[SA_MAX];
	t_gene		g;
	t_gene		best;
	int			bestc;
	int			pipos[SA_MAX];
	int			spos[SA_MAX];
	int			tpos[SA_MAX];
	t_state		ck[2][SA_MAX + 1];
	int			pre[2][SA_MAX + 1];
	int			finish;
	int			cur;
	t_u64		rng;
	double		temp;
	double		logu[SA_LOGU];
	int			d;
	int			limit;
	long		work;
	t_rot		rot;
	int			len;
	int			buf[SA_MAX];
	int			buf2[SA_MAX];
	int			save[SA_MAX];
	t_state		sa;
	t_state		sb;
	int			alive[SA_MAX];
	int			tops[SA_MAX];
	int			bots[SA_MAX];
	int			lap[SA_MAX];
	int			side[SA_MAX];
	int			nt;
	int			nb;
	int			top;
	int			pick;
	int			pick_d;
	int			val[SA_MAX];
	int			dp[SA_MAX];
	int			par[SA_MAX];
	t_arm		arms[SA_ARMS];
	int			order[SA_ARMS];
	t_gene		saved;
	t_solution	*out;
}	t_sa;

/* sa_bits.c */
int		sa_cyc(const t_u64 *b, int from, int to, int dom);
int		sa_next_set(const t_u64 *b, int c, int dom);
void	sa_bit_set(t_u64 *b, int i);
void	sa_bit_clear(t_u64 *b, int i);
int		sa_bit_get(const t_u64 *b, int i);
int		sa_popcount(t_u64 v);
int		sa_ctz(t_u64 v);

/* sa_math.c */
double	sa_ln(double v);
double	sa_exp(double v);
double	sa_temp_at(double t0, double t1, double frac);
void	sa_init_rng(t_sa *x);
t_uint	sa_rnd(t_sa *x);

/* sa_step.c */
void	sa_choose_dir(t_rot *r);
int		sa_step(t_state *s, const int *c, t_rot *r);
int		sa_step_at(t_sa *x, t_state *s, int phase, int e);

/* sa_run.c */
void	sa_start0(t_sa *x, t_state *s);
void	sa_start1(t_sa *x, t_state *s, const t_state *end0);
int		sa_run(t_sa *x, t_state *s, int phase, int save);
int		sa_finish_cost(t_sa *x, const t_state *s);
int		sa_eval(t_sa *x, int save);

/* sa_span.c */
int		sa_bounded(t_sa *x, t_state *s, const t_span *sp);
void	sa_refresh(t_sa *x, t_lm *l);
void	sa_reindex(t_sa *x);
void	sa_icopy(int *dst, const int *src, int n);
int		sa_accept(t_sa *x, int delta);

/* sa_emit.c */
void	sa_emit_solution(t_sa *x);

/* sa_keep.c */
void	sa_init_keep(t_sa *x);

/* sa_seed.c */
void	sa_ring(t_sa *x, const t_ring *r);

/* sa_greedy.c */
void	sa_greedy_tau(t_sa *x);

/* sa_pick.c */
int		sa_pick_dist(t_sa *x, int m);
int		sa_pick_target(t_sa *x, int m, int from);
int		sa_guided_target(t_sa *x, int py, int m, int from);
int		sa_rank_neighbour(t_sa *x, int e);
int		sa_pos_neighbour(t_sa *x, int e);

/* sa_order.c */
void	sa_relocate(t_sa *x, int *arr, int from, int to);
void	sa_reverse(int *arr, int i, int j);
void	sa_arr_remove(int *a, int m, int v);
void	sa_arr_insert(int *a, int m, int v, int at);

/* sa_local_prep.c */
void	sa_prep_reverse(t_sa *x, t_lm *l);
int		sa_prep_relocate(t_sa *x, t_lm *l, int guided);

/* sa_local.c */
void	sa_try_local(t_sa *x, int phase);

/* sa_pi.c */
void	sa_try_pi(t_sa *x);

/* sa_pi_delta.c */
int		sa_pi_delta(t_sa *x, int to, int lo, int hi);

/* sa_pair.c */
void	sa_try_pair(t_sa *x, int mode);

/* sa_toggle.c */
void	sa_try_toggle(t_sa *x);

/* sa_anneal.c */
void	sa_anneal(t_sa *x, long work, double t0, double t1);

/* sa_arms.c */
void	sa_build_arms(t_sa *x);

/* sa_race.c */
void	sa_race(t_sa *x, long work);

/* sa_solve.c */
void	sa_solve(t_stack *a, t_solution *sol);

/* swapify.c */
void	swapify(t_solution *sol);

#endif
